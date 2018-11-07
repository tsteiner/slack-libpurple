#include <string.h>

#include <request.h>
#include <debug.h>

#include "slack-json.h"
#include "slack-channel.h"
#include "slack-user.h"
#include "slack-api.h"
#include "slack-message.h"
#include "slack-conversation.h"
#include "slack-blist.h"

void slack_blist_uncache(SlackAccount *sa, PurpleBlistNode *b) {
	const char *bid = purple_blist_node_get_string(b, SLACK_BLIST_KEY);
	if (bid)
		g_hash_table_remove(sa->buddies, bid);
	purple_blist_node_remove_setting(b, SLACK_BLIST_KEY);
}

void slack_blist_cache(SlackAccount *sa, PurpleBlistNode *b, const char *id) {
	if (id)
		purple_blist_node_set_string(b, SLACK_BLIST_KEY, id);
	const char *bid = purple_blist_node_get_string(b, SLACK_BLIST_KEY);
	if (bid)
		g_hash_table_insert(sa->buddies, (gpointer)bid, b);
}

void slack_buddy_free(PurpleBuddy *b) {
	/* This should be unnecessary, as there's no analogue for PurpleChat so we have to deal with cleanup elsewhere anyway */
	SlackAccount *sa = get_slack_account(b->account);
	if (sa) slack_blist_uncache(sa, &b->node);
}

#define PURPLE_BLIST_ACCOUNT(n) \
	( PURPLE_BLIST_NODE_IS_BUDDY(n) \
		? PURPLE_BUDDY(n)->account \
	: PURPLE_BLIST_NODE_IS_CHAT(n) \
		? PURPLE_CHAT(n)->account \
		: NULL)

SlackObject *slack_blist_node_get_obj(PurpleBlistNode *buddy, SlackAccount **sap) {
	*sap = get_slack_account(PURPLE_BLIST_ACCOUNT(buddy));
	if (!*sap)
		return NULL;
	if (PURPLE_BLIST_NODE_IS_BUDDY(buddy))
		return g_hash_table_lookup((*sap)->user_names, purple_buddy_get_name(PURPLE_BUDDY(buddy)));
	else if (PURPLE_BLIST_NODE_IS_CHAT(buddy))
		return g_hash_table_lookup((*sap)->channel_names, purple_chat_get_name(PURPLE_CHAT(buddy)));
	return NULL;
}

void slack_blist_init(SlackAccount *sa) {
	char *id = sa->team.id ?: "";
	if (!sa->blist) {
		PurpleBlistNode *g;
		for (g = purple_blist_get_root(); g; g = purple_blist_node_next(g, TRUE)) {
			const char *bid;
			if (PURPLE_BLIST_NODE_IS_GROUP(g) &&
					(bid = purple_blist_node_get_string(g, SLACK_BLIST_KEY)) &&
					!strcmp(bid, id)) {
				sa->blist = PURPLE_GROUP(g);
				break;
			}
		}
		if (!sa->blist) {
			sa->blist = purple_group_new(sa->team.name ?: "Slack");
			purple_blist_node_set_string(&sa->blist->node, SLACK_BLIST_KEY, id);
			purple_blist_add_group(sa->blist, NULL);
		}
	}

	/* Find all leaf nodes on this account (buddies and chats) with slack ids and cache them */
	PurpleBlistNode *node;
	for (node = purple_blist_get_root(); node; node = node->next) {
		while (node->child)
			node = node->child;

		if (PURPLE_BLIST_ACCOUNT(node) == sa->account)
			slack_blist_cache(sa, node, NULL);

		while (node->parent && !node->next)
			node = node->parent;
	}
}

PurpleChat *slack_find_blist_chat(PurpleAccount *account, const char *name) {
	SlackAccount *sa = get_slack_account(account);
	if (sa && sa->channel_names) {
		SlackChannel *chan = g_hash_table_lookup(sa->channel_names, name);
		if (chan && chan->object.buddy)
			return channel_buddy(chan);
	}
	return NULL;
}

static void get_history_cb(PurpleBlistNode *buddy, PurpleRequestFields *fields) {
	SlackAccount *sa;
	SlackObject *obj = slack_blist_node_get_obj(buddy, &sa);
	g_return_if_fail(obj);

	int count = purple_request_fields_get_integer(fields, "count");
	if (count > 0)
		slack_get_history(sa, obj, NULL, count);
	else
		slack_get_conversation_unread(sa, obj);
}

static void get_history_prompt(PurpleBlistNode *buddy) {
	SlackAccount *sa = get_slack_account(PURPLE_BLIST_ACCOUNT(buddy));
	const char *name = PURPLE_BLIST_NODE_NAME(buddy);
	g_return_if_fail(sa && name);

	PurpleRequestFields *fields = purple_request_fields_new();
	PurpleRequestFieldGroup *group = purple_request_field_group_new("NULL");
	PurpleRequestField *field = purple_request_field_int_new("count", "Count (0 for unread)", 100);
	purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);
	purple_request_fields_add_group(fields, group);
	gchar *primary = g_strdup_printf("Retrieve message history for %c%s", PURPLE_BLIST_NODE_IS_BUDDY(buddy) ? '@' : '#', name);
	PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_BLIST_NODE_IS_BUDDY(buddy) ? PURPLE_CONV_TYPE_IM : PURPLE_CONV_TYPE_CHAT, name, sa->account);
	purple_request_fields(sa->gc, "Get History", primary, NULL, fields, "Get", G_CALLBACK(get_history_cb), "Cancel", NULL, sa->account, name, conv, buddy);
	g_free(primary);
}

GList *slack_blist_node_menu(PurpleBlistNode *buddy) {
	GList *menu = NULL;
	SlackAccount *sa = get_slack_account(PURPLE_BLIST_ACCOUNT(buddy));

	if (sa) {
		menu = g_list_append(menu, purple_menu_action_new("Get history", G_CALLBACK(get_history_prompt), buddy, NULL));
	}
	
	return menu;
}

struct roomlist_expand {
	PurpleRoomlist *list;
	PurpleRoomlistRoom *parent;
	SlackChannelType type;
	gboolean archived;
};

void free_roomlist_expand(struct roomlist_expand *expand) {
	purple_roomlist_unref(expand->list);
	g_free(expand);
}

static void roomlist_cb(SlackAccount *sa, gpointer data, json_value *json, const char *error) {
	struct roomlist_expand *expand = data;

	json = json_get_prop_type(json, expand->type >= SLACK_CHANNEL_GROUP ? "groups" : "channels", array);
	if (!json || error) {
		purple_notify_error(sa->gc, "Channel list error", "Could not read channel list", error);
        purple_roomlist_set_in_progress(expand->list, FALSE);
		free_roomlist_expand(expand);
		return;
	}

	for (unsigned i = 0; i < json->u.array.length; i++) {
		json_value *chan = json->u.array.values[i];

		gboolean archived = json_get_prop_boolean(chan, "is_archived", FALSE) || json_get_prop_boolean(chan, "is_deleted", FALSE);
		if (archived != expand->archived)
			continue;

		PurpleRoomlistRoom *room = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_ROOM, json_get_prop_strptr(chan, "name"), expand->parent);
		purple_roomlist_room_add_field(expand->list, room, json_get_prop_strptr(chan, "id"));
		purple_roomlist_room_add_field(expand->list, room, json_get_prop_strptr(json_get_prop(chan, "topic"), "value"));
		purple_roomlist_room_add_field(expand->list, room, json_get_prop_strptr(json_get_prop(chan, "purpose"), "value"));
		purple_roomlist_room_add_field(expand->list, room, GUINT_TO_POINTER((gulong) json_get_val(json_get_prop(chan, "num_members"), integer, 0)));
		time_t t = slack_parse_time(json_get_prop(chan, "created"));
		purple_roomlist_room_add_field(expand->list, room, purple_date_format_long(localtime(&t)));
		SlackUser *creator = (SlackUser*)slack_object_hash_table_lookup(sa->users, json_get_prop_strptr(chan, "creator"));
		purple_roomlist_room_add_field(expand->list, room, creator ? creator->object.name : NULL);
		purple_roomlist_room_add(expand->list, room);
	}

    purple_roomlist_set_in_progress(expand->list, FALSE);
	free_roomlist_expand(expand);
}

void slack_roomlist_expand_category(PurpleRoomlist *list, PurpleRoomlistRoom *parent) {
	SlackAccount *sa = get_slack_account(list->account);
	if (!sa)
		return;

	g_warn_if_fail(list == sa->roomlist);

	struct roomlist_expand *expand = g_new0(struct roomlist_expand, 1);
	expand->list = list;
	expand->parent = parent;
	const char *cat = purple_roomlist_room_get_name(parent);
	if (!g_strcmp0(cat, "Archived") && parent->parent) {
		expand->archived = TRUE;
		cat = purple_roomlist_room_get_name(parent->parent);
	}
	const char *op;
	if (!g_strcmp0(cat, "Public Channels")) {
		expand->type = SLACK_CHANNEL_PUBLIC;
		op = "channels.list";
	}
	else if (!g_strcmp0(cat, "Private Channels")) {
		expand->type = SLACK_CHANNEL_GROUP;
		op = "groups.list";
	}
	else if (!g_strcmp0(cat, "Multiparty Direct Messages")) {
		expand->type = SLACK_CHANNEL_MPIM;
		op = "mpim.list";
	}
	else
		return;
	purple_roomlist_ref(list);
    purple_roomlist_set_in_progress(list, TRUE);
	slack_api_call(sa, roomlist_cb, expand, op, "exclude_archived", expand->archived ? "false" : "true", "exclude_members", "true", NULL);
}

PurpleRoomlist *slack_roomlist_get_list(PurpleConnection *gc) {
	SlackAccount *sa = gc->proto_data;

	if (sa->roomlist)
		purple_roomlist_unref(sa->roomlist);

	PurpleRoomlist *list = sa->roomlist = purple_roomlist_new(sa->account);

	GList *fields = NULL;
	fields = g_list_append(fields, purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, "ID", "id", TRUE));
	fields = g_list_append(fields, purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, "Topic", "topic", FALSE));
	fields = g_list_append(fields, purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, "Purpose", "purpose", FALSE));
	fields = g_list_append(fields, purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_INT, "Members", "members", FALSE));
	fields = g_list_append(fields, purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, "Created", "created", FALSE));
	fields = g_list_append(fields, purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, "Creator", "creator", FALSE));
	purple_roomlist_set_fields(list, fields);

	PurpleRoomlistRoom *public = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_CATEGORY, "Public Channels", NULL);
	purple_roomlist_room_add(list, public);
	PurpleRoomlistRoom *private = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_CATEGORY, "Private Channels", NULL);
	purple_roomlist_room_add(list, private);
	purple_roomlist_room_add(list, purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_CATEGORY, "Multiparty Direct Messages", NULL));
	purple_roomlist_room_add(list, purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_CATEGORY, "Archived", public));
	purple_roomlist_room_add(list, purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_CATEGORY, "Archived", private));

	return list;
}

void slack_roomlist_cancel(PurpleRoomlist *list) {
	SlackAccount *sa = get_slack_account(list->account);
	if (!sa)
		return;

	if (sa->roomlist == list) {
		purple_roomlist_unref(list);
		sa->roomlist = NULL;
	}
}

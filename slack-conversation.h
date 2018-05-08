#ifndef _PURPLE_SLACK_CONVERSATION_H
#define _PURPLE_SLACK_CONVERSATION_H

#include "json.h"
#include "slack.h"
#include "slack-object.h"
#include "slack-channel.h"
#include "slack-user.h"

static inline const char *slack_conversation_id(SlackObject *chan) {
	g_return_val_if_fail(chan, NULL);
	if (SLACK_IS_CHANNEL(chan))
		return chan->id;
	if (SLACK_IS_USER(chan))
		return ((SlackUser*)chan)->im;
	return NULL;
}

static inline SlackObject *slack_conversation_lookup_id(SlackAccount *sa, const slack_object_id id) {
	return g_hash_table_lookup(sa->channels, id) ?: g_hash_table_lookup(sa->ims, id);
}

static inline SlackObject *slack_conversation_lookup_sid(SlackAccount *sa, const char *sid) {
	if (!sid)
		return NULL;
	slack_object_id id;
	slack_object_id_set(id, sid);
	return slack_conversation_lookup_id(sa, id);
}

/** @name Initialization */
void slack_conversations_load(SlackAccount *sa);

/** @name API */
SlackObject *slack_conversation_get_conversation(SlackAccount *sa, PurpleConversation *conv);
void slack_mark_conversation(SlackAccount *sa, PurpleConversation *conv);

/**
 * Retrieve and display history for a conversation
 *
 * @param since oldest message to display (NULL for beginning of time)
 * @param count maximum number of messages to display
 */
void slack_get_history(SlackAccount *sa, SlackObject *conv, const char *since, unsigned count);

/**
 * Retrieve and display unread history for a conversation
 *
 * @param json json object for the conversation (including last_read, unread_count)
 */
void slack_get_history_unread(SlackAccount *sa, SlackObject *conv, json_value *json);

/**
 * Retrieve and display unread history for a conversation
 */
void slack_get_conversation_unread(SlackAccount *sa, SlackObject *conv);

#endif // _PURPLE_SLACK_CONVERSATION_H

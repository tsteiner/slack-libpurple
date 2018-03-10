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

/* Initialization */
void slack_conversations_load(SlackAccount *sa);

SlackObject *slack_conversation_get_conversation(SlackAccount *sa, PurpleConversation *conv);

#endif // _PURPLE_SLACK_CONVERSATION_H

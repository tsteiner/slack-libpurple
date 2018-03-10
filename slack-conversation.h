#ifndef _PURPLE_SLACK_CONVERSATION_H
#define _PURPLE_SLACK_CONVERSATION_H

#include "json.h"
#include "slack-object.h"
#include "slack.h"

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

#endif // _PURPLE_SLACK_CONVERSATION_H

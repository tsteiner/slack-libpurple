# slack-libpurple

A Slack protocol plugin for libpurple IM clients.

Here's how slack concepts are mapped to purple:

   * Your "open" channels (on the slack bar) are mapped to the buddy list: joining a channel is equivalent to creating a buddy
   * Which conversations are open in purple is up to you, and has no effect on slack... (how to deal with activity in open channels with no conversation?)
   * TBD... feedback welcome

## Installation/Configuration

1. Install libpurple (pidgin, finch, etc.), obviously
1. Run `make install` (or `make install-user`)
1. [Issue a Slack API token](https://api.slack.com/custom-integrations/legacy-tokens) for yourself
1. Add your slack account to your libpurple program and enter this token under (Advanced) API token (do *not* enter your slack password; username/hostname are optional but can be set to `you@your.slack.com`)

If you're using a front-end (like Adium or Spectrum2) that does not let you set the API token, you can enter your token as the account password instead.

## Status

- [x] Basic IM (direct message) functionality
- [x] Basic channel (chat) functionality
- [ ] Apply buddy changes to open/close channels
- [x] Proper message formatting (for @mentions and such, incoming only)
- [x] Set presence/status (text only)
- [x] Retrieve message history to populate new conversations (channels only)?
- [ ] Images/icons?
- [ ] File transfers
- [ ] Optimize HTTP connections (libpurple 3?)

#pragma once
#include "Types.hpp"
#include "WavPlayer.hpp"

using VoiceMsgId = size_t;

/*!
 * \brief Play wav from table.
 * \param table Wav samples table.
 * \param id Sample id.
 */
void VoiceMsgPlay(const samples_table_t *table, VoiceMsgId id);
/*!
 * \brief Stop playback.
 */
void VoiceMsgStop();
/*!
 * \brief Blocking wait playback stop.
 * \param xTicks Ticks to wait.
 * \return Timeout result.
 */
bool VoiceMsgWaitStop(size_t xTicks);
/*!
 * \brief Mute player.
 * \param state on or off.
 */
void VoiceMsgMutePlayer(bool state);

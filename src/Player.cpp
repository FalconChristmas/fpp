/*
 *   Player Class for Falcon Pi Player (FPP)
 *
 *   Copyright (C) 2013 the Falcon Pi Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Pi Player (FPP) is free software; you can redistribute it
 *   and/or modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/select.h>
#include <unistd.h>

#ifdef USEHTTPAPI
#  include "httpAPI.h"
#endif

#ifdef USEWIRINGPI
#   include <wiringPi.h>
#   include <piFace.h>
#endif

#include "channeloutput.h"
#include "command.h"
#include "common.h"
#include "controlsend.h"
#include "controlrecv.h"
#include "e131bridge.h"
#include "effects.h"
#include "events.h"
#include "fpp.h"
#include "fppd.h"
#include "gpio.h"
#include "log.h"
#include "mediaoutput.h"
#include "PixelOverlay.h"
#include "Player.h"
#include "Plugins.h"
#include "Scheduler.h"
#include "Sequence.h"
#include "settings.h"

/////////////////////////////////////////////////////////////////////////////
Player *player = NULL;

Player::Player()
  : m_scheduler(NULL),
	m_runMainLoop(0),
	m_nextSequenceID(1),
	m_refreshRate(20),
	m_defaultLightDelay(50000),
	m_lightDelay(50000),
	m_masterFramesPlayed(-1),
	m_outputFrames(1),
	m_mediaOffset(0.0),
	m_sequencesPaused(0),
	m_outputThreadID(0),
	m_runOutputThread(1),
	m_outputThreadIsRunning(0),
	m_seqLastControlMajor(0),
	m_seqLastControlMinor(0)
{
	pthread_mutex_init(&m_sequenceLock, NULL);
}

Player::~Player()
{
	pthread_mutex_destroy(&m_sequenceLock);

	m_sequence.clear();
}

// Default is 50000 for 50ms normally, 1000000 == 1 second
#define MAIN_LOOP_SLEEP_US 50000

void Player::MainLoop(void)
{
	int            commandSock = 0;
	int            controlSock = 0;
	int            bridgeSock = 0;
	int            prevFPPstatus = FPPstatus;
	int            sleepUs = MAIN_LOOP_SLEEP_US;
	fd_set         active_fd_set;
	fd_set         read_fd_set;
	struct timeval timeout;
	int            selectResult;

	LogDebug(VB_PLAYER, "Player::MainLoop()\n");

	if (getFPPmode() & PLAYER_MODE)
	{
		m_scheduler = new Scheduler(this);

		if (!m_scheduler)
		{
			LogErr(VB_PLAYER, "Error creating Scheduler\n");
			return;
		}

		m_newPlaylist = new NewPlaylist(this);

		if (!m_newPlaylist)
		{
			LogErr(VB_PLAYER, "Error creating Playlist\n");
			return;
		}
	}

	m_runMainLoop = 1;

	FD_ZERO (&active_fd_set);

	CheckExistanceOfDirectoriesAndFiles();

#ifdef USEWIRINGPI
	wiringPiSetupGpio(); // would prefer wiringPiSetupSys();
	piFaceSetup(200); // PiFace inputs 1-8 == wiringPi 200-207
#endif

	if (getFPPmode() == BRIDGE_MODE)
	{
		bridgeSock = Bridge_Initialize();
		if (bridgeSock)
			FD_SET (bridgeSock, &active_fd_set);
	}
	else
	{
		InitMediaOutput();
	}

	pluginCallbackManager.init();

	InitializeChannelOutputs();
	SendBlankingData();

	InitEffects();
	InitializeChannelDataMemoryMap();

	commandSock = Command_Initialize();
	if (commandSock)
		FD_SET (commandSock, &active_fd_set);

#ifdef USEHTTPAPI
	APIServer apiServer;
	apiServer.Init();
#endif

	controlSock = InitControlSocket();
	FD_SET (controlSock, &active_fd_set);

	SetupGPIOInput();

	if (getFPPmode() & PLAYER_MODE)
	{
		if (getFPPmode() == MASTER_MODE)
			InitSyncMaster();

		m_scheduler->CheckIfShouldBePlayingNow();
	}

	if ((getAlwaysTransmit()) ||
		(UsingMemoryMapInput()) ||
		(getFPPmode() & BRIDGE_MODE))
		StartChannelOutputThread();

	LogInfo(VB_PLAYER, "Starting main processing loop\n");

	while (m_runMainLoop)
	{
		timeout.tv_sec  = 0;
		timeout.tv_usec = sleepUs;

		read_fd_set = active_fd_set;


		selectResult = select(FD_SETSIZE, &read_fd_set, NULL, NULL, &timeout);
		if (selectResult < 0)
		{
			if (errno == EINTR)
			{
				// We get interrupted when media players finish
				continue;
			}
			else
			{
				LogErr(VB_PLAYER, "Main select() failed: %s\n",
					strerror(errno));
				m_runMainLoop = 0;
				continue;
			}
		}

		if (commandSock && FD_ISSET(commandSock, &read_fd_set))
			CommandProc();

		if (bridgeSock && FD_ISSET(bridgeSock, &read_fd_set))
			Bridge_ReceiveData();

		if (controlSock && FD_ISSET(controlSock, &read_fd_set))
			ProcessControlPacket();

		// Check to see if we need to start up the output thread.
		// FIXME, possibly trigger this via a fpp command to fppd
		if ((!ChannelOutputThreadIsRunning()) &&
			(getFPPmode() != BRIDGE_MODE) &&
			((UsingMemoryMapInput()) ||
			 (channelTester->Testing()) ||
			 (getAlwaysTransmit()))) {
			int E131BridgingInterval = getSettingInt("E131BridgingInterval");
			if (!E131BridgingInterval)
				E131BridgingInterval = 50;
			SetChannelOutputRefreshRate(1000 / E131BridgingInterval);
			StartChannelOutputThread();
		}

		if (getFPPmode() & PLAYER_MODE)
		{
			if ((FPPstatus == FPP_STATUS_PLAYLIST_PLAYING) ||
				(FPPstatus == FPP_STATUS_STOPPING_NOW) ||
				(FPPstatus == FPP_STATUS_STOPPING_GRACEFULLY_AFTER_LOOP) ||
				(FPPstatus == FPP_STATUS_STOPPING_GRACEFULLY))
			{
//				if (prevFPPstatus == FPP_STATUS_IDLE)
//				{
//					m_newPlaylist->Start();
//					sleepUs = 10000;
// FIXME PLAYLIST
// sleepUs = 500000;
//				}

				// Check again here in case PlayListPlayingInit
				// didn't find anything and put us back to IDLE
				if ((FPPstatus == FPP_STATUS_PLAYLIST_PLAYING) ||
					(FPPstatus == FPP_STATUS_STOPPING_NOW) ||
					(FPPstatus == FPP_STATUS_STOPPING_GRACEFULLY_AFTER_LOOP) ||
					(FPPstatus == FPP_STATUS_STOPPING_GRACEFULLY))
				{
					m_newPlaylist->Process();
				}
			}

			int reactivated = 0;
			if (FPPstatus == FPP_STATUS_IDLE)
			{
				if ((prevFPPstatus == FPP_STATUS_PLAYLIST_PLAYING) ||
					(prevFPPstatus == FPP_STATUS_STOPPING_NOW) ||
					(prevFPPstatus == FPP_STATUS_STOPPING_GRACEFULLY_AFTER_LOOP) ||
					(prevFPPstatus == FPP_STATUS_STOPPING_GRACEFULLY))
				{
					m_newPlaylist->Cleanup();

					m_scheduler->ReLoadCurrentScheduleInfo();

					if (FPPstatus != FPP_STATUS_IDLE)
						reactivated = 1;
					else
						sleepUs = MAIN_LOOP_SLEEP_US;
				}
			}

			if (reactivated)
				prevFPPstatus = FPP_STATUS_IDLE;
			else
				prevFPPstatus = FPPstatus;

			m_scheduler->ScheduleProc();
		}
		else if (getFPPmode() == REMOTE_MODE)
		{
			if(mediaOutputStatus.status == MEDIAOUTPUTSTATUS_PLAYING)
			{
// FIXME playlist
// when in remote mode, who owns the sequences and mediaoutputs we are playing?
//				playlist->PlaylistProcessMediaData();
LogDebug(VB_PLAYER, "FIXME PLAYLIST\n");
			}
		}

		CheckGPIOInputs();
	}

	StopChannelOutputThread();
	ShutdownControlSocket();

	if (getFPPmode() == BRIDGE_MODE)
	{
		Bridge_Shutdown();
	}
	else if (getFPPmode() & PLAYER_MODE)
	{
		delete m_scheduler;
		delete m_newPlaylist;
	}

	LogInfo(VB_PLAYER, "Main Loop complete, shutting down.\n");
}

void Player::Shutdown(void)
{
	m_runMainLoop = 0;
}

/*
 *
 */
int Player::PlaylistStart(std::string playlistName, int position, int repeat)
{
	LogDebug(VB_PLAYER, "Player::PlaylistStart(%s, %d, %d)\n",
		playlistName.c_str(), position, repeat);

	if (!m_newPlaylist)
		return 0;

	if (FPPstatus != FPP_STATUS_IDLE)
	{
		m_newPlaylist->StopNow();
		usleep(500000);
	}

	if (!m_newPlaylist->Load(playlistName.c_str()))
		return 0;

	m_newPlaylist->SetPosition(position);
	m_newPlaylist->SetRepeat(repeat);

	if (!m_newPlaylist->Start())
		return 0;
	
	return 1;
}

/*
 *
 */
int Player::PlaylistStopNow(void)
{
	if (!m_newPlaylist)
		return 0;

	return m_newPlaylist->StopNow();
}

/*
 *
 */
int Player::PlaylistStopGracefully(int afterCurrentLoop)
{
	if (!m_newPlaylist)
		return 0;

	return m_newPlaylist->StopGracefully(afterCurrentLoop);
}

void Player::GetNextScheduleStartText(char *txt)
{
	if (m_scheduler)
		m_scheduler->GetNextScheduleStartText(txt);
}

void Player::GetNextPlaylistText(char *txt)
{
	if (m_scheduler)
		m_scheduler->GetNextPlaylistText(txt);
}

void Player::ReLoadCurrentScheduleInfo(void)
{
	if (m_scheduler)
		m_scheduler->ReLoadCurrentScheduleInfo();
}

void Player::ReLoadNextScheduleInfo(void)
{
	if (m_scheduler)
		m_scheduler->ReLoadNextScheduleInfo();
}

void Player::SendBlankingData(void)
{
	usleep(100000); // FIXME, figure out a way to not need this delay
	                // possibly by just starting thread up once more

	bzero(m_seqData, FPPD_MAX_CHANNELS);

	if (getFPPmode() == MASTER_MODE)
		SendBlankingDataPacket();

	SendChannelData(m_seqData);
}

int Player::GetPlaybackSecondsElapsed(void)
{
	int result = 0;

	pthread_mutex_lock(&m_sequenceLock);
	if (m_sequence.size())
		result = m_sequence.back()->m_seqSecondsElapsed;
	pthread_mutex_unlock(&m_sequenceLock);

	return result;
}

int Player::GetPlaybackSecondsRemaining(void)
{
	int result = 0;

	pthread_mutex_lock(&m_sequenceLock);
	if (m_sequence.size())
		result = m_sequence.back()->m_seqSecondsRemaining;
	pthread_mutex_unlock(&m_sequenceLock);

	return result;
}

int Player::StartMedia(std::string mediaName, int newMediaOffset)
{
	int mediaOffsetInt = getSettingInt("mediaOffset");

	if (mediaOffsetInt || newMediaOffset)
		m_mediaOffset = (float)(mediaOffsetInt + newMediaOffset) * 0.001;
	else
		m_mediaOffset = 0.0;

	OpenMediaOutput(mediaName.c_str());
}

int Player::StopMedia(void)
{
	CloseMediaOutput();
}

long long Player::StartSequence(std::string seqName, int priority, int startSeconds,
	int startChannel, int blockSize, int autoRepeat)
{
	LogDebug(VB_PLAYER, "Player::StartSequence(%s, %d, %d, %d, %d, %d)\n",
		seqName.c_str(), priority, startSeconds, startChannel, blockSize,
		autoRepeat);
	pthread_mutex_lock(&m_sequenceLock);

	Sequence *newSequence = new Sequence(priority, startChannel, blockSize);

	if ((!m_nextSequenceID) ||
		(m_nextSequenceID < 0))
		m_nextSequenceID = 1;

	newSequence->SetSequenceID(m_nextSequenceID++);

	if (autoRepeat)
		newSequence->SetAutoRepeat();

	newSequence->OpenSequenceFile(seqName, startSeconds);

	int inserted = 0;
	if (m_sequence.size())
	{
		// Order the vector from lowest to highest priority.  In the event
		// of a tie, the newest item is put at the high end.
		for (int i = 0; i < m_sequence.size(); i++)
		{
			if (m_sequence[i]->GetPriority() > priority)
			{
				m_sequence.insert(m_sequence.begin() + i, newSequence);
				inserted = 1;
				break;
			}
		}
	}

	if (!inserted)
	{
		m_sequence.push_back(newSequence);
	    SetChannelOutputRefreshRate(newSequence->GetRefreshRate());
	}

	pthread_mutex_unlock(&m_sequenceLock);

	if (!m_outputThreadIsRunning)
	    StartChannelOutputThread();

	return newSequence->GetSequenceID();
}

int Player::StopSequence(std::string seqName)
{
	LogDebug(VB_PLAYER, "Player::StopSequence(%s)\n", seqName.c_str());

	pthread_mutex_lock(&m_sequenceLock);

	for (int i = 0; i < m_sequence.size(); i++)
	{
		if (m_sequence[i]->m_seqFilename == seqName)
		{
			Sequence *delSequence = m_sequence[i];

			delSequence->CloseSequenceFile();
			delete delSequence;

			m_sequence.erase(m_sequence.begin() + i);
			break;
		}
	}

	pthread_mutex_unlock(&m_sequenceLock);
}

int Player::StopAllSequences(void)
{
	LogDebug(VB_PLAYER, "Player::StopAllSequence()\n");

	pthread_mutex_lock(&m_sequenceLock);

	while (!m_sequence.empty())
	{
		Sequence *delSequence = m_sequence.back();

		delSequence->CloseSequenceFile();
		delete delSequence;

		m_sequence.pop_back();
	}

	pthread_mutex_unlock(&m_sequenceLock);
}

int Player::SeekSequence(std::string seqName, int frameNumber)
{
	LogDebug(VB_PLAYER, "Player::StopSequence(%s)\n", seqName.c_str());
	int found = 0;

	pthread_mutex_lock(&m_sequenceLock);

	for (int i = 0; i < m_sequence.size(); i++)
	{
		if (m_sequence[i]->m_seqFilename == seqName)
		{
			m_sequence[i]->SeekSequenceFile(frameNumber);
			found = 1;
			break;
		}
	}

	pthread_mutex_unlock(&m_sequenceLock);

	return found;
}

int Player::SequencesRunning(void)
{
	pthread_mutex_lock(&m_sequenceLock);
	int result = m_sequence.size();
	pthread_mutex_unlock(&m_sequenceLock);

	return result;
}

/*
 *
 */
int Player::SequenceIsRunning(long long id)
{
	int found = 0;

	pthread_mutex_lock(&m_sequenceLock);

	for (int i = 0; i < m_sequence.size() && !found; i++)
	{
		if (m_sequence[i]->m_sequenceID == id)
			found = 1;
	}
	pthread_mutex_unlock(&m_sequenceLock);
	
	return found;
}

/*
 * Check to see if the channel output thread is running
 */
int Player::ChannelOutputThreadIsRunning(void) {
	return m_outputThreadIsRunning;
}

/*
 *
 */
void Player::DisableChannelOutput(void) {
	LogDebug(VB_PLAYER | VB_CHANNELOUT, "DisableChannelOutput()\n");
	m_outputFrames = 0;
}

/*
 *
 */
void Player::EnableChannelOutput(void) {
	LogDebug(VB_PLAYER | VB_CHANNELOUT, "EnableChannelOutput()\n");
	m_outputFrames = 1;
}

/*
 * Main loop in channel output thread
 */
void Player::RunChannelOutputThread(void)
{
	static long long lastStatTime = 0;
	long long startTime;
	long long sendTime;
	long long readTime;
	int onceMore = 0;
	struct timespec ts;
	int syncFrameCounter = 0;

	LogDebug(VB_PLAYER | VB_CHANNELOUT, "RunChannelOutputThread() starting\n");

	m_outputThreadIsRunning = 1;

	if ((getFPPmode() == REMOTE_MODE) &&
		(!IsEffectRunning()) &&
		(!UsingMemoryMapInput()) &&
		(!channelTester->Testing()) &&
		(!getAlwaysTransmit()))
	{
		// Sleep about 2 seconds waiting for the master
		int loops = 0;
		while ((m_masterFramesPlayed < 0) && (loops < 200))
		{
			usleep(10000);
			loops++;
		}

		// Stop playback if the master hasn't sent any sync packets yet
		if (m_masterFramesPlayed < 0)
			m_runOutputThread = 0;
	}

	StartOutputThreads();

	while (m_runOutputThread)
	{
		startTime = GetTime();

		pthread_mutex_lock(&m_sequenceLock);
		int runningSequences = m_sequence.size();

		if ((getFPPmode() == MASTER_MODE) && (runningSequences))
		{
			if (syncFrameCounter & 0x10)
			{
				// Send sync every 16 frames (use 16 to make the check simpler)
				syncFrameCounter = 1;

				for (int i = 0; i < m_sequence.size(); i++)
					SendSeqSyncPacket( m_sequence[i]->m_seqFilename,
						channelOutputFrame, mediaElapsedSeconds);
			}
			else
			{
				syncFrameCounter++;
			}
		}

		if (m_outputFrames)
		{
			if (getFPPmode() == BRIDGE_MODE)
			{
				memcpy(m_seqData, e131Data, FPPD_MAX_CHANNELS);
			}
			else
			{
				for (int i = 0; i < m_sequence.size(); i++)
					m_sequence[i]->OverlayNextFrame(m_seqData);
			}

			SendChannelData(m_seqData);
		}

		sendTime = GetTime();

		if ((getFPPmode() != BRIDGE_MODE) &&
			(runningSequences))
		{
			// Close any sequences that aren't open anymore
			for (int i = m_sequence.size() - 1; i >= 0; i--)
			{
				if (!m_sequence[i]->SequenceFileOpen())
				{
					Sequence *seq = m_sequence[i];
					m_sequence.erase(m_sequence.begin() + i);
					delete seq;
				}
			}

			runningSequences = m_sequence.size();

			// Loop through sequences pre-reading next frame of data
			for (int i = 0; i < m_sequence.size(); i++)
			{
				m_sequence[i]->ReadSequenceData();
			}
		}

		ProcessChannelData();

		readTime = GetTime();

		pthread_mutex_unlock(&m_sequenceLock);

		if ((runningSequences) ||
			(IsEffectRunning()) ||
			(UsingMemoryMapInput()) ||
			(channelTester->Testing()) ||
			(getAlwaysTransmit()) ||
			(getFPPmode() == BRIDGE_MODE))
		{
			onceMore = 1;

			if (startTime > (lastStatTime + 1000000)) {
				int sleepTime = m_lightDelay - (GetTime() - startTime);
				if (sleepTime < 0)
					sleepTime = 0;
				lastStatTime = startTime;
				LogDebug(VB_PLAYER | VB_CHANNELOUT,
					"Output Thread: Loop: %dus, Send: %lldus, Read: %lldus, Sleep: %dus, FrameNum: %ld\n",
					m_lightDelay, sendTime - startTime,
					readTime - sendTime, sleepTime, channelOutputFrame);
			}
		}
		else
		{
			m_lightDelay = m_defaultLightDelay;

			if (onceMore)
				onceMore = 0;
			else
				m_runOutputThread = 0;
		}

		// Calculate how long we need to nanosleep()
		ts.tv_sec = 0;
		ts.tv_nsec = (m_lightDelay - (GetTime() - startTime)) * 1000;
		nanosleep(&ts, NULL);
	}

	StopOutputThreads();

	m_outputThreadIsRunning = 0;

	LogDebug(VB_PLAYER | VB_CHANNELOUT, "RunChannelOutputThread() completed\n");

	pthread_exit(NULL);
}

/*
 *
 */
void Player::ProcessChannelData(void)
{
	if (IsEffectRunning())
		OverlayEffects(m_seqData);

	if (UsingMemoryMapInput())
		OverlayMemoryMap(m_seqData);

	if (getControlMajor() && getControlMinor())
	{
		char thisMajor = NormalizeControlValue(m_seqData[getControlMajor()-1]);
		char thisMinor = NormalizeControlValue(m_seqData[getControlMinor()-1]);

		if ((m_seqLastControlMajor != thisMajor) ||
			(m_seqLastControlMinor != thisMinor))
		{
			m_seqLastControlMajor = thisMajor;
			m_seqLastControlMinor = thisMinor;

			if (m_seqLastControlMajor && m_seqLastControlMinor)
				TriggerEvent(m_seqLastControlMajor, m_seqLastControlMinor);
		}
	}

	if (channelTester->Testing())
		channelTester->OverlayTestData(m_seqData);
}

/*
 * Normalize control channel values into buckets
 */
char Player::NormalizeControlValue(char in)
{
	char result = (char)(((unsigned char)in + 5) / 10);

	if (result == 26)
		return 25;

	return result;
}

/*
 * Set the step time
 */
void Player::SetChannelOutputRefreshRate(int rate)
{
	m_refreshRate = rate;
	m_defaultLightDelay = 1000000 / m_refreshRate;
}

void *ChannelOutputThread(void *data)
{
	Player *playerInst = reinterpret_cast<Player*>(data);
	playerInst->RunChannelOutputThread();
}

/*
 * Kick off the channel output thread
 */
int Player::StartChannelOutputThread(void)
{
	LogDebug(VB_PLAYER | VB_CHANNELOUT, "StartChannelOutputThread()\n");
	if (!ChannelOutputThreadIsRunning())
	{
		// Give a little time in case we were shutting down
		usleep(200000);
		if (ChannelOutputThreadIsRunning())
		{
			LogDebug(VB_PLAYER | VB_CHANNELOUT, "Channel Output thread is already running\n");
			return 1;
		}
	}

	int mediaOffsetInt = getSettingInt("mediaOffset");
	if (mediaOffsetInt)
		m_mediaOffset = (float)mediaOffsetInt * 0.001;
	else
		m_mediaOffset = 0.0;

	LogDebug(VB_PLAYER | VB_MEDIAOUT, "Using mediaOffset of %.3f\n", m_mediaOffset);

	int E131BridgingInterval = getSettingInt("E131BridgingInterval");

	if ((getFPPmode() == BRIDGE_MODE) && (E131BridgingInterval))
		m_defaultLightDelay = E131BridgingInterval * 1000;
	else
		m_defaultLightDelay = 1000000 / m_refreshRate;

	m_lightDelay = m_defaultLightDelay;

	m_runOutputThread = 1;
	int result = pthread_create(&m_outputThreadID, NULL, &ChannelOutputThread, this);

	if (result)
	{
		char msg[256];

		m_runOutputThread = 0;

		switch (result)
		{
			case EAGAIN: strcpy(msg, "Insufficient Resources");
				break;
			case EINVAL: strcpy(msg, "Invalid settings");
				break;
			case EPERM : strcpy(msg, "Invalid Permissions");
				break;
		}

		LogErr(VB_PLAYER | VB_CHANNELOUT, "ERROR creating channel output thread: %s\n", msg );
		return 0;
	}
	else
	{
		pthread_detach(m_outputThreadID);
	}

	// Wait for thread to start
	while (!ChannelOutputThreadIsRunning())
		usleep(10000);

	return 1;
}

/*
 *
 */
int Player::StopChannelOutputThread(void)
{
	int i = 0;

	// Stop the thread and wait a few seconds
	m_runOutputThread = 0;
	while (m_outputThreadIsRunning && (i < 5))
	{
		sleep(1);
		i++;
	}

	// Didn't stop for some reason, so it was hung somewhere
	if (m_outputThreadIsRunning)
		return -1;

	return 0;
}

/*
 *
 */
int Player::ToggleSequencePause(void)
{
	if (m_sequencesPaused)
		m_sequencesPaused = 0;
	else
		m_sequencesPaused = 1;

	pthread_mutex_lock(&m_sequenceLock);
	for (int i = 0; i < m_sequence.size(); i++)
		m_sequence[i]->SetPauseState(m_sequencesPaused);

	pthread_mutex_unlock(&m_sequenceLock);

	return m_sequencesPaused;
}

/*
 *
 */
void Player::SingleStepSequences(void)
{
	pthread_mutex_lock(&m_sequenceLock);
	for (int i = 0; i < m_sequence.size(); i++)
		m_sequence[i]->SingleStepSequence();

	pthread_mutex_unlock(&m_sequenceLock);
}

/*
 *
 */
void Player::SingleStepSequencesBack(void)
{
	pthread_mutex_lock(&m_sequenceLock);
	for (int i = 0; i < m_sequence.size(); i++)
		m_sequence[i]->SingleStepSequenceBack();

	pthread_mutex_unlock(&m_sequenceLock);
}

/*
 *
 */
void Player::NextPlaylistItem(void)
{
	m_newPlaylist->NextItem();
}

/*
 *
 */
void Player::PrevPlaylistItem(void)
{
	m_newPlaylist->PrevItem();
}

/*
 *
 */
Json::Value Player::GetCurrentPlaylistInfo(void)
{
	return m_newPlaylist->GetInfo();
}

/*
 *
 */
Json::Value Player::GetCurrentPlaylistEntry(void)
{
	return m_newPlaylist->GetCurrentEntry();
}

/*
 * Reset the master frames played position
 */
void Player::ResetMasterPosition(void)
{
	m_masterFramesPlayed = -1;
}

/*
 * Update the count of frames that the master has played so we can sync to it
 */
void Player::UpdateMasterPosition(int frameNumber)
{
	m_masterFramesPlayed = frameNumber;
	CalculateNewChannelOutputDelayForFrame(frameNumber);
}

/*
 * Calculate the new sync offset based on the current position reported
 * by the media player.
 */
void Player::CalculateNewChannelOutputDelay(float mediaPosition)
{
	static float nextSyncCheck = 0.5;

	if (getFPPmode() == REMOTE_MODE)
		return;

	if ((mediaPosition <= nextSyncCheck) &&
		(nextSyncCheck < (mediaPosition + 2.0)))
		return;

	float offsetMediaPosition = mediaPosition - m_mediaOffset;

	int expectedFramesSent = (int)(offsetMediaPosition * m_refreshRate);

	mediaElapsedSeconds = mediaPosition;

	LogDebug(VB_PLAYER | VB_CHANNELOUT,
		"Media Position: %.2f, Offset: %.3f, Frames Sent: %d, Expected: %d, Diff: %d\n",
		mediaPosition, m_mediaOffset, channelOutputFrame, expectedFramesSent,
		channelOutputFrame - expectedFramesSent);

	CalculateNewChannelOutputDelayForFrame(expectedFramesSent);
}

/*
 * Calculate the new sync offset based on a desired frame number
 */
void Player::CalculateNewChannelOutputDelayForFrame(int expectedFramesSent)
{
	int diff = channelOutputFrame - expectedFramesSent;
	if (diff)
	{
		int timerOffset = diff * (m_defaultLightDelay / 100);
		int newLightDelay = m_lightDelay;

		if (channelOutputFrame >  expectedFramesSent)
		{
			// correct if we slingshot past 0, otherwise offset further
			if (m_lightDelay < m_defaultLightDelay)
				newLightDelay = m_defaultLightDelay;
			else
				newLightDelay += timerOffset;
		}
		else
		{
			// correct if we slingshot past 0, otherwise offset further
			if (m_lightDelay > m_defaultLightDelay)
				newLightDelay = m_defaultLightDelay;
			else
				newLightDelay += timerOffset;
		}

		// Don't let us go more than 15ms out from the default.  If we
		// can't keep up using that delta then we probably won't be able to.
		if ((m_defaultLightDelay - 15000) > newLightDelay)
			newLightDelay = m_defaultLightDelay - 15000;

		LogDebug(VB_PLAYER | VB_CHANNELOUT, "LightDelay: %d, newLightDelay: %d\n",
			m_lightDelay, newLightDelay);
		m_lightDelay = newLightDelay;
	}
	else if (m_lightDelay != m_defaultLightDelay)
	{
		m_lightDelay = m_defaultLightDelay;
	}
}

/*
 *
 */
Json::Value Player::GetCurrentPlaylist(void)
{
	if (!m_newPlaylist)
	{
		Json::Value result;
		return result;
	}

	return m_newPlaylist->GetConfig();
}


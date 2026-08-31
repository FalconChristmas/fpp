#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2025 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

// AES67Manager — GStreamer-based AES67 audio-over-IP send/receive
//
// Replaces the previous PipeWire RTP module approach with GStreamer pipelines
// and uses ptp4l (linuxptp) for IEEE 1588 PTP clock synchronization on the
// network, achieving true AES67 compliance.
//
// Features:
//   - Send: pipewiresrc → audioconvert → rtpL24pay → udpsink
//   - Receive: udpsrc → rtpjitterbuffer → rtpL24depay → audioconvert → pipewiresink
//   - ptp4l for IEEE 1588 PTP on the network (grandmaster or follower via BMCA)
//   - Built-in SAP announcer (replaces external fpp_aes67_sap Python daemon)
//   - SAP receiver for inbound stream discovery
//   - Config format: same pipewire-aes67-instances.json (backward-compatible)

#if __has_include(<gst/gst.h>)
#define HAS_AES67_GSTREAMER

#include <gst/gst.h>

#include "fpphttp_types.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// ──────────────────────────────────────────────────────────────────────────────
// AES67 Protocol Constants (mirror fpp_aes67_common.py)
// ──────────────────────────────────────────────────────────────────────────────
namespace AES67 {

constexpr int RTP_PAYLOAD_TYPE      = 96;
constexpr int AUDIO_RATE            = 48000;
constexpr int AUDIO_RTP_TTL         = 4;
constexpr int DEFAULT_PTIME_MS      = 4;        // 4ms packet time
constexpr int DEFAULT_PORT          = 5004;
constexpr int DEFAULT_CHANNELS      = 2;
constexpr int DEFAULT_LATENCY_MS    = 10;

// DSCP codepoints (AES67-2018 / AES-R16 QoS recommendations)
constexpr int AUDIO_DSCP             = 34;      // AF41 -- RTP audio (udpsink qos-dscp)
constexpr int PTP_DSCP                = 46;     // EF   -- PTP event/general messages (ptp4l dscp_event/dscp_general)

// PTP (IEEE 1588) profile defaults
constexpr int DEFAULT_PTP_DOMAIN     = 0;

// BMCA priority1 values selected by the "ptpRole" setting.  Most professional
// AES67 gear ships priority1=128, so an FPP box left at 128 ties on priority
// and wins on clock identity (lowest MAC) -- silently hijacking the clock
// domain of a console or DSP.  See issue #2848.
constexpr int PTP_PRIORITY_PREFER_MASTER = 127; // deliberately beat the usual 128
constexpr int PTP_PRIORITY_AUTO          = 248; // yield to real gear; still GM when alone

// How long the SAP announcer re-checks the grandmaster every second after
// startup, to catch BMCA settling rather than waiting a whole announce cycle.
constexpr int PTP_CONVERGENCE_WINDOW_S   = 60;

// TTL for the cached pmc query result.  /aes67/status is HTTP-facing and each
// query forks a pmc process, so repeated hits must not fork per request.
constexpr int PTP_QUERY_CACHE_MS         = 1000;

constexpr const char* DEFAULT_MULTICAST_IP = "239.69.0.1";
constexpr const char* AUDIO_FORMAT         = "S24BE";
constexpr const char* SAP_MCAST_ADDRESS    = "239.255.255.255";
constexpr int SAP_PORT                     = 9875;
constexpr int SAP_VERSION                  = 1;
constexpr int SAP_ANNOUNCE_INTERVAL_S      = 30;
constexpr int SAP_TTL                      = 255;

// Valid AES67 ptimes
inline bool IsValidPtime(int ptime) { return ptime == 1 || ptime == 4; }

// Channel position names for SDP
const char* GetSDPChannelNames(int channels);

} // namespace AES67

// ──────────────────────────────────────────────────────────────────────────────
// Instance configuration — mirrors the JSON schema
// ──────────────────────────────────────────────────────────────────────────────
struct AES67Instance {
    int id = 0;
    std::string name;
    bool enabled = true;
    std::string mode = "send";           // "send" | "receive" | "both"
    std::string multicastIP = AES67::DEFAULT_MULTICAST_IP;
    int port = AES67::DEFAULT_PORT;
    int channels = AES67::DEFAULT_CHANNELS;
    std::string interface;               // network interface (e.g., "eth0")
    std::string sessionName;
    int latency = AES67::DEFAULT_LATENCY_MS;
    bool sapEnabled = true;
    int ptime = AES67::DEFAULT_PTIME_MS;
};

struct AES67Config {
    std::vector<AES67Instance> instances;
    bool ptpEnabled = true;
    std::string ptpInterface = "eth0";
    int ptpDomain = AES67::DEFAULT_PTP_DOMAIN;

    // BMCA participation:
    //   "auto"     (default) -- priority1 248: still becomes grandmaster when
    //                           it is the only clock on the domain, but yields
    //                           to gear that wants the role.
    //   "follower" -- ptp4l slaveOnly: never becomes grandmaster.
    //   "master"   -- priority1 127: prefer to win the election.
    std::string ptpRole = "auto";

    // Diagnostic switches for the AES67 send path.  Both default on; set
    // either to false in pipewire-aes67-instances.json and hit Apply to take
    // that change out of the pipeline without rebuilding, which is what makes
    // a sender problem bisectable against real receiver hardware.
    //
    //   ptpMediaClock -- drive the pipeline from PTP time and derive the RTP
    //       timestamps from it (AES67 "a=mediaclk:direct=0").
    //   sourcePacing  -- ask PipeWire for a packet-sized quantum so RTP leaves
    //       evenly instead of one graph quantum at a time.
    bool ptpMediaClock = true;
    // Default OFF, and do not turn it on without solving what follows.
    //
    // It does fix the transmit cadence -- no gaps over 8ms in a 500-packet
    // capture, and a Dante receiver's subscription state goes green -- but the
    // audio is severely distorted, on both a 44100 and a genuine 48000 graph
    // (confirmed on Yamaha MRX7-D hardware, issue #2848).  The fractional
    // quantum theory (192/48000 being 176.4 samples at 44100) is therefore
    // wrong: it distorts just as badly where the request divides exactly.
    //
    // The cause is still unknown.  It is not packet loss or timeline damage:
    // sequence is unbroken, timestamp increments are exact, and the payload is
    // statistically normal music with no splices at packet boundaries.  The
    // corruption is inside the packets, which is why no packet-level metric
    // catches it.
    //
    // Note the priority this deserves: the bursty default path plays cleanly on
    // real hardware even at the MRX7-D's minimum 0.25ms receive latency, so the
    // cadence this would fix is not currently blocking anything.
    //
    // Prefer sinkPacing below, which fixes the same cadence without touching
    // the graph and measurably does not distort.  The distortion this one
    // causes is now measurable too -- scripts/aes67_verify decodes the RTP
    // payload and correlates it against the source file, which is the check
    // that was missing when the note above was written.
    bool sourcePacing = false;

    // Pace the packets at the sink instead of at the source.
    //
    // The default path bursts: pipewiresrc hands over one graph quantum at a
    // time and udpsink (sync=false) puts the whole lot on the wire in
    // microseconds.  Measured on a 1024-sample/44100 graph that is 5.8 packets
    // back-to-back every 23.22ms -- 83% of packets arrive under 1ms after the
    // one before, then nothing for 23ms.
    //
    // Which receivers that breaks depends entirely on how they clock playout.
    // Hardware that places samples by RTP timestamp against its own PTP time
    // does not care when a packet arrived, only that it arrived before its
    // deadline -- which is why a Yamaha MRX7-D plays this cleanly even at
    // 0.25ms.  Software monitors generally play out by arrival into a small
    // jitter buffer, so a 23ms silence underruns them every quantum and the
    // audio fragments.  Both reports on issue #2848 are the same defect seen
    // from those two sides.
    //
    // sourcePacing fixes the cadence by shrinking the quantum, but it shrinks
    // it for the whole graph -- the sound card included -- and the audio comes
    // out distorted.  This does not touch the graph: the queue below gives the
    // sink its own thread, so it can wait for each packet's running time
    // without the backpressure reaching the live source.  That backpressure is
    // what cost ~36% of the stream the last time sync=true was tried without a
    // queue in between.
    //
    // sinkPacingMs is how long each packet is held back so that it is still in
    // the future when the sink gets it.  It must exceed one graph quantum
    // (23.22ms at 1024/44100) or every buffer arrives late and goes out
    // immediately, which is the burst all over again.  40ms leaves real margin
    // on top of that on purpose: too small a value does not fail loudly, it
    // silently reverts to bursting, which is expensive to diagnose.  It costs
    // sender latency, but the receiver no longer has to absorb the burst as
    // link offset.
    //
    // Measured on a 1024/44100 graph, 4ms ptime, against the source audio:
    //
    //                      default path        sinkPacing
    //   median gap         0.028ms             3.999ms
    //   packets per burst  5.8                 1.00
    //   back-to-back       83%                 0.0%
    //   content vs source  0.995 correlation   1.000 correlation
    //
    // Unlike sourcePacing it does not touch the graph, so it does not distort.
    //
    // It is NOT yet a fix on its own, and default OFF for a concrete reason:
    // the queue drains.  The sink releases packets on the PTP clock while the
    // audio arrives on the sound card crystal, and on this hardware those are
    // 56ppm apart (see adaptiveResample).  Starting from a 40-60ms queue that
    // is 12-18 minutes to empty, which matches what was measured: cadence held
    // at exactly 4.000ms and 250.2 packets/s for ~15 minutes, then the queue
    // touched 0 and in that same instant the median gap went to 0.031ms, 60%
    // of packets went back-to-back and the rate fell to 187.6/s.  It does not
    // recover -- with nothing buffered every packet arrives past its send time
    // and udpsink dumps them all immediately.
    //
    // So sink pacing is correct but not self-sufficient: it needs the audio
    // rate locked to the PTP clock, i.e. a working adaptiveResample, or the
    // queue empties and it fails worse than not pacing at all.  Raising
    // sinkPacingMs only buys time proportionally; it does not fix a systematic
    // rate difference.
    //
    // Verified with scripts/aes67_verify, not yet against Dante or Yamaha
    // hardware.
    bool sinkPacing = false;
    int sinkPacingMs = 40;

    // Reconcile the audio clock with the PTP clock by inserting or dropping
    // samples, instead of by varying a resampler's rate.
    //
    // The drift is a fixed ~56ppm between the sound card's crystal and the
    // NIC's PHC (see adaptiveResample), and both variable-rate elements that
    // could correct it have been measured and rejected -- "speed" and "pitch"
    // each accept a rate property and neither applies it to a live stream.
    //
    // "audiorate" attacks it from the other side.  It does not resample: it
    // compares each buffer's timestamp against the sample count it has already
    // emitted and inserts or drops samples to make the two agree.  Since
    // pipewiresrc timestamps buffers on the pipeline clock (the PHC) while the
    // sample count comes from the card, that difference IS the drift, so the
    // correction needs no control loop, no gains and no dead time -- the thing
    // that made the previous attempt oscillate.
    //
    // It also settles the actuation question directly: "add" and "drop" are
    // readable sample counters, so whether it is doing anything is a fact
    // rather than an inference from a property read-back.  At 56ppm expect
    // ~2.7 samples/s of correction.
    //
    // rateMatchToleranceNs is how far the two may diverge before it acts.  0
    // corrects continuously; the element's own default of 40ms lets the error
    // build and then dumps a 40ms correction, which is plainly audible.
    //
    // MEASURED, AND IT DOES NOT WORK.  Keep it default OFF.  audiorate is the
    // third rejected actuator, after "speed" and "pitch", and it fails
    // differently from both -- it actuates, but wrongly, and then unrecoverably:
    //
    //   - It over-corrects by more than an order of magnitude.  Steady state it
    //     inserted ~40 samples/s (833ppm) where 2.7 samples/s (56ppm) was
    //     needed.  It is reconciling per-buffer timestamp rounding, not drift,
    //     and no tolerance from 0 to 5ms changed that.
    //   - After ~11 minutes it enters permanent runaway, inserting ~12,000
    //     samples/s -- a quarter of the stream becomes fabricated silence, and
    //     it never recovers.  Reproduced twice, at 10-12 and 11 minutes, on an
    //     otherwise healthy system.  The shape fits a single forward jump in
    //     the timeline leaving a deficit it then chases forever.
    //
    // What this experiment DID establish, and the reason the switch is kept:
    // with the rate corrected, sinkPacing's queue stops draining.  Over 28
    // minutes the cadence held at 4.00ms with uniform 1152-byte payloads and
    // 250.0 packets/s, and the queue oscillated 56-80ms with no downward trend,
    // against a collapse at ~15 minutes without it.  So "correct the sample
    // rate, then pace at the sink" is the right architecture; what is missing
    // is only a correct rate corrector.
    //
    // That corrector needs to (a) apply a slow, near-constant ~56ppm
    // correction rather than tracking per-buffer jitter, (b) preserve exact
    // ptime blocking, and (c) reset rather than accumulate across a timeline
    // discontinuity.  No stock GStreamer element on the Pi image does all
    // three, so it likely has to be written -- a fractional-delay resampler
    // with its own read pointer, which keeps every output buffer exactly one
    // packet long while consuming slightly more or fewer input samples.
    bool rateMatch = false;
    guint64 rateMatchToleranceNs = 0;

    // Correct the sound card / PHC rate difference with libsamplerate, which
    // is what the three rejected elements above were each trying to be.
    //
    // See the DriftResampleProbe comment in AES67Manager.cpp for the control
    // law.  In short: the ratio is computed directly from (input frames / PHC
    // seconds) rather than servoed towards a target, because that quantity is
    // the difference between two crystals and is therefore near-constant --
    // which is why this does not oscillate the way the earlier loops did.
    //
    // Requires libsamplerate at build time; without it the switch does nothing
    // and the send path behaves exactly as before.
    //
    // MEASURED.  The resampler itself is transparent and the correction is
    // right, but this is default OFF because it does not survive past ~11
    // minutes -- see below.
    //
    // Transparency, measured warp-insensitively by comparing power spectra
    // against the source file (a 57ppm warp moves a bin by 0.006%, so a PSD
    // sees straight through it): OFF gives mean -3.01 dB / stdev 0.00 dB, ON
    // gives mean -3.01 dB / stdev 0.09 dB.  Indistinguishable.
    //
    // Do NOT judge this with correlation against the source: the stream is
    // deliberately time-warped, so a fixed alignment cannot match a whole
    // window and correlation reads ~0.93 on audio that is provably clean.  It
    // recovers as the window shrinks -- 0.42 at 2s, 0.85 at 100ms, tracking
    // intra-window slip -- which is a property of the measurement, not the
    // audio.
    //
    // Correction: trim converges to +57ppm and holds, leaving ~2.5ppm residual
    // against 56ppm uncorrected, with the alignment slipping exactly -5.5
    // samples per 2s as intended.
    //
    // With sinkPacing this produced the first fully correct AES67 stream in
    // this file's history: 4.000ms cadence, 1.00 packets per burst, 250.1
    // packets/s, uniform 1152-byte payloads, pacing queue stable at 56-80ms.
    // That held for 9-11 minutes and then degraded -- send rate falling to
    // 155/s with ragged payloads, and the media timeline stepping by +/-23.2ms,
    // which is exactly one graph quantum at 1024/44100.
    //
    // That degradation is NOT this code.  The same ~11 minute onset appeared
    // with audiorate, and sinkPacing alone collapsed at ~15 minutes; the media
    // file is 220 minutes long and playback was never restarted, so it is
    // neither a track boundary nor a restart.  Something starves the AES67
    // branch after ten-odd minutes of playback and that is the next thing to
    // find -- it is very likely the original "sounds fine then goes fragmented"
    // report on #2848, which every fix so far has merely postponed.
    bool driftResample = false;

    // Count bytes in at pipewiresrc and out at udpsink, and log both rates.
    //
    // Diagnostic for the send rate collapsing partway through a long playback
    // (250/s to 188/s at ~11 minutes, then to 151/s at ~19 minutes, with
    // PipeWire reporting no xruns and CPU, memory, thread and fd counts all
    // flat).  Since PipeWire says it is delivering, the audio is going missing
    // between pipewiresrc and udpsink, and these two counters say which side
    // of the pipeline loses it.
    bool pipelineStats = false;

    // Take audio from PipeWire at the graph's own rate and convert to the AES67
    // rate inside GStreamer, instead of asking PipeWire to deliver 48kHz from a
    // 44.1kHz graph.
    //
    // This is the fault behind "AES67 sounds fine, then goes fragmented".  With
    // a 44100 graph, pipewiresrc negotiates 48000 and PipeWire resamples for
    // that node alone; measured at the pipewiresrc pad, delivery holds at
    // 287,968 B/s for 13 minutes, then steps to exactly 0.750x, then to 0.600x,
    // and stays there.  PipeWire reports no xruns for the node throughout, and
    // CPU, memory, threads, fds, temperature and the graph quantum are all flat
    // across the whole run.
    //
    // Our own pipeline is not involved: byte counters at pipewiresrc and
    // udpsink track each other at a constant 1.010 ratio the entire time, so
    // nothing downstream loses a sample -- there is simply less audio arriving.
    // The physical sinks are unaffected because they run at the graph rate and
    // are never resampled, which is exactly why the sound card stays clean
    // while the AES67 stream falls apart.
    //
    // Default OFF: this was written as a candidate fix for the collapse and it
    // is not one -- delivery degrades identically whether the source is taken
    // at 48kHz or at the graph's own rate.  PipeWire's per-node resampling is
    // not at fault either way; pw-record's own 48k-from-44.1k stream runs at
    // 99.9% of realtime indefinitely.  Kept as a knob, not a fix.
    bool nativeSourceRate = false;

    // Copy each buffer out of PipeWire's pool instead of referencing it, and
    // negotiate a deeper pool.
    //
    // Candidate fix for the send rate collapsing partway through playback.
    // Delivery to pipewiresrc steps to exactly 3/4 then 3/5 of nominal and
    // stays there, with no xruns on the node and every resource metric flat --
    // the shape of PipeWire skipping cycles because no buffer is free to fill.
    // pipewiresrc negotiates min-buffers=2 and, with always-copy off, a buffer
    // is only returned to the pool once everything downstream has dropped its
    // reference, so anything that holds one starves the next cycle.
    //
    // Ruled out first, so this is not a guess at the level above: the graph is
    // healthy (recording the sound card's monitor gives 99.9% of realtime while
    // AES67 is at 52%), our own pipeline loses nothing (byte counters at
    // pipewiresrc and udpsink hold a constant 1.010 ratio), and it is not
    // per-node resampling (it degrades identically at the graph's native rate,
    // and pw-record's own 48k-from-44.1k stream is unaffected).
    // THIS is the fix.  Isolated by A/B: a 16 buffer pool on its own holds the
    // send rate flat for 26 minutes, with always-copy off and the source still
    // taken at 48kHz.  So the copy is not needed -- the pool was simply too
    // shallow to absorb normal downstream reference-holding, and PipeWire
    // skipped the cycles it could not fill.
    //
    // always-copy remains available for the case where something downstream
    // genuinely retains buffers, at the cost of a memcpy per buffer.
    bool sourceBufferCopy = false;
    int sourceMinBuffers = 16;

    // Schedule the capture node from PTP instead of from the sound card.
    //
    // This is the producer-side half of the pool problem.  pipewiresrc receives
    // into sourceMinBuffers slots that PipeWire fills at the graph's rate --
    // the card crystal, ~54ppm slow of PTP here -- while the pipeline drains
    // them against the PTP clock.  driftResample corrects the rate downstream
    // of the pool, so it cannot refill it; the pool empties after (N-1) quanta
    // and the source then skips a cycle per rotation for good.
    //
    // PipeWire supports exactly this case.  A support.node.driver clocked from
    // the NIC's PHC drives any node that joins its node.group, and PipeWire
    // rate-matches at the boundary with the card-driven graph using the
    // adapter's DLL-driven adaptive resampler -- upstream of our pool rather
    // than downstream of it.  It is the same mechanism pipewire-aes67.conf
    // uses, where the comment on aes67.driver-group calls it "force rate
    // matching on the AES67 node rather than other nodes".
    //
    // Requires a driver node to exist with a matching group; FPP ships one in
    // 98-fpp-ptp-driver.conf.  Harmless if absent -- the node keeps the graph
    // driver and behaves exactly as before.
    bool sourcePtpGroup = false;
    std::string sourcePtpGroupName = "pipewire.ptp0";

    // Schedule the capture node asynchronously from its producer.  Required
    // whenever sourcePtpGroup is on: crossing driver domains without it stalls
    // the stream outright.  See the comment at the node.async call site.
    bool sourceAsync = true;

    // Adaptive resampling: continuously trim the send stream's rate so the
    // media timeline advances at exactly PTP rate.
    //
    // Without it the audio is clocked by the sound card's crystal while the RTP
    // timestamps advance on PTP, so the two walk apart -- measured at -111ppm,
    // i.e. 6.7ms per minute.  A receiver holding 5ms (the Yamaha MRX7-D's
    // maximum) runs out in under a minute and mutes; at its 0.25ms minimum, in
    // seconds.  That is why AES67 audio plays cleanly right after a restart and
    // then dies, which cost a lot of confusing test reports on #2848.
    // Default OFF: the loop is implemented but NOT yet working.  With the
    // gains it shipped with it drove the stream to +890ppm (worse than the
    // -111ppm it corrects); with the gains fixed it now makes no correction at
    // all -- measured -112.6ppm against a -110.9ppm uncorrected baseline.
    //
    // Next step is to find out which half is broken, which needs the
    // per-iteration numbers this loop already logs at debug level: either
    // gst_element_query_position() is not reporting the post-"speed" media
    // timeline (so `ratio` is always ~1 and no correction is ever computed), or
    // the speed property is not taking effect on a running pipeline.  Check
    // `ratio` in the log first: if it tracks the real drift the measurement is
    // fine and the element is at fault, and vice versa.
    //
    // UPDATE: the "speed" element is not a working actuator.  Driven from 0 to
    // the full -500ppm clamp on a live stereo stream, the measured drift moved
    // by ~35ppm (-80 -> -45) where ~500ppm was asked for.  It accepts the
    // property and reads it back -- which is what earlier notes here wrongly
    // took as proof of actuation -- but does not apply it to a live source,
    // having been written for changing playback rate of seekable media.
    //
    // The measurement half of the loop IS sound: on a stereo stream it reads a
    // steady -40 to -100ppm, consistent with the -110.9ppm measured
    // independently from packet captures.  So what needs replacing is the
    // actuator, not the control loop.
    //
    // "pitch" (soundtouch) was the next candidate and it is also not usable:
    //
    //   - It does not actuate either.  Over six actuations the trim ramped
    //     83ppm (+11.8 -> -71.4) while the measured ratio just wandered around
    //     -50ppm with +/-35ppm of noise -- no systematic response, the same
    //     signature "speed" gave.
    //   - It destroys the transmit cadence.  soundtouch emits in large internal
    //     blocks, so everything downstream arrives late in clumps: with pitch in
    //     the chain, 16.05 packets per burst and 93.8% back-to-back with the
    //     pacing queue pinned at 0, against 1.00 packets and 0.0% with it
    //     removed (A/B on the same track, same build).
    //
    // Why this matters more than it looks: this is not a nice-to-have.  The
    // drift is two free-running crystals -- the NIC's PHC against the sound
    // card's -- measured at -55.8ppm over 10s, -56.36ppm over 60s and -56.9ppm
    // from packet captures, three independent methods agreeing to ~1ppm.  When
    // FPP is PTP master (as it is by default with no other grandmaster on the
    // network) ptp4l never disciplines the PHC at all, so there is nothing to
    // lock to and no PTP setting can remove this.  It also drains the sinkPacing
    // queue, so both the drift and the pacing failure are this one cause.
    //
    // What is still needed is a variable-rate resampler that (a) actually
    // responds to a rate property on a live stream and (b) preserves buffer
    // cadence.  Neither "speed" nor "pitch" does both.
    bool adaptiveResample = false;
};

// ──────────────────────────────────────────────────────────────────────────────
// Pipeline wrapper — one per send or receive direction of an instance
// ──────────────────────────────────────────────────────────────────────────────
struct AES67Pipeline {
    int instanceId = 0;
    bool isSend = true;
    GstElement* pipeline = nullptr;
    GstElement* rtpbin = nullptr;
    GstBus* bus = nullptr;          // polled by watchdog (no GLib main loop)

    // Pipeline state
    bool running = false;
    std::string errorMessage;

    // Buffer-drop probe: when > 0, the probe drops buffers and
    // decrements the counter.  Used by FlushSendPipelines() to
    // discard stale audio between tracks.
    std::atomic<int> dropCounter = 0;
    GstPad* probePad = nullptr;          // pad where probe is installed
    gulong probeId = 0;                  // installed probe handle (0 = none)

    // Zombie detection: track bytes pushed by udpsink across watchdog cycles.
    // If bytes haven't increased for 2 consecutive checks while pipeline
    // reports PLAYING, the pipewiresrc has lost its PipeWire connection.
    uint64_t lastByteCount = 0;
    int     stallCount = 0;              // consecutive checks with no progress
    // Under-delivery detection.  A stall is easy -- bytes stop entirely -- but
    // the failure that actually bites is partial: after roughly 105-110
    // minutes of pipeline uptime the send rate settles around 90% of nominal
    // with periodic RTP timestamp gaps, and it never recovers on its own, so
    // the stream plays broken until fppd is restarted.  bytes-served keeps
    // climbing throughout, so the stall check above cannot see it.
    //
    // The cause is the source buffer pool draining, and the interval is set by
    // clock drift, not by uptime.  pipewiresrc receives into sourceMinBuffers
    // slots while the pipeline consumes against PTP; the PipeWire graph clock
    // free-runs ~54ppm slow of the NIC's PHC on this hardware, so the pool
    // loses that difference steadily and empties after (N-1) quanta of
    // accumulated slip.  From then on the source skips exactly one graph cycle
    // every N buffers, forever, until the pipeline is rebuilt with a full pool.
    //
    // Measured both ways round, which is what confirmed it:
    //
    //   min-buffers  onset buffer   onset    gap spacing   implied drift
    //   16           273985         97.4m    every 16      54.7 ppm
    //    8           128604         45.7m    every  8      54.4 ppm
    //
    // against 54.2ppm measured directly with phc_ctl.  Onset ratio 2.130 vs
    // 15/7 = 2.143 predicted.  Forcing the graph to 48000 shifts the onset to
    // 98 minutes from 108 -- 10% off in wall time, but the same buffer count,
    // which is what first showed the trigger counts cycles rather than seconds.
    //
    // So sourceMinBuffers is a reservoir, not a fix: it sets how long the
    // stream survives, not whether it degrades.  Raising it buys proportional
    // time and nothing else.  A reporter whose clock drifts ~17ppm sees the
    // same failure at 5h18m for the same reason.
    //
    // Earlier notes here read this as uptime-driven.  That was fitted to runs
    // that happened to contain no restarts; the cadence in fact continues
    // straight through an fppd restart, which is what ruled uptime out.
    std::chrono::steady_clock::time_point lastByteTime{};
    int     lowRateCount = 0;
    int     channels = AES67::DEFAULT_CHANNELS;  // for the expected byte rate
    bool    pacingTuned = false;         // latency-derived shift applied yet?
};

// ──────────────────────────────────────────────────────────────────────────────
// SAP discovered stream (from remote announcements)
// ──────────────────────────────────────────────────────────────────────────────
struct SAPDiscoveredStream {
    uint16_t msgIdHash = 0;
    std::string originAddress;
    std::string sessionName;
    std::string multicastIP;
    int port = 0;
    int channels = 2;
    int ptime = 4;
    std::string ptpClockId;
    uint64_t lastSeenMs = 0;
    int autoCreatedInstanceId = -1;      // ID of auto-created receive pipeline, or -1
};

// ──────────────────────────────────────────────────────────────────────────────
// AES67Manager — singleton managing all AES67 GStreamer pipelines
// ──────────────────────────────────────────────────────────────────────────────
class AES67Manager {
public:
    static AES67Manager& INSTANCE;

    // HTTP API endpoint (registered at /aes67)
    HttpResponsePtr render_GET(const HttpRequestPtr& req);

    // Lifecycle
    bool Init();                         // Called from fppd startup
    void Shutdown();                     // Called from fppd shutdown

    // Apply configuration — reads JSON and rebuilds pipelines
    // Called from PHP API (POST /api/pipewire/aes67/apply) and boot
    bool ApplyConfig();

    // Cleanup — stop all pipelines, remove PipeWire configs
    void Cleanup();

    // Status query — for PHP API (GET /api/pipewire/aes67/status)
    struct Status {
        struct PipelineStatus {
            int instanceId;
            std::string name;
            std::string mode;        // "send" or "receive"
            bool running;
            std::string error;
            // Rate pipewiresrc actually negotiated with the graph, before our
            // resampler.  48000 means the audio reaches AES67 untouched; any
            // other value means we are rate-converting off the sound card's
            // clock.  This is the number that matters for AES67 -- the graph's
            // default.clock.rate does not tell you what this stream is fed,
            // because per-card and per-group rates sit in between.
            int sourceRate = 0;
        };
        std::vector<PipelineStatus> pipelines;
        bool ptpSynced = false;
        int64_t ptpOffsetNs = 0;     // offset from PTP master
        std::string ptpGrandmasterId;
        std::string ptpPortState;    // ptp4l portState (MASTER/SLAVE/LISTENING/...)
        bool ptpIsGrandmaster = false;  // true when *we* hold the grandmaster role
        // Copied out of m_config under m_configMutex so the HTTP handler can
        // render them without touching shared config itself.
        bool ptpEnabled = false;
        int ptpDomain = 0;
        std::string ptpRole;
        // The graph's clock rate, for context only -- see PipelineStatus::sourceRate
        // for what actually feeds a given AES67 stream.
        int graphSampleRate = 0;
        std::vector<SAPDiscoveredStream> discoveredStreams;
    };
    Status GetStatus();

    // Self-test — validates AES67 subsystem components.
    // Returns JSON with pass/fail for each test.
    struct TestResult {
        std::string testName;
        bool passed;
        std::string message;
    };
    std::vector<TestResult> RunSelfTest();

    // Signal from fppd that PipeWire is ready (called after PipeWire restart)
    void OnPipeWireReady();

    // Check if manager is active
    bool IsActive() const { return m_active.load(); }

    // Returns true if there are active AES67 send instances that want
    // zero-hop direct RTP branches from media pipelines.
    bool HasActiveSendInstances();

    // Zero-hop optimization (7.9): Attach RTP send branches directly to an
    // existing GStreamer tee element inside a media pipeline, bypassing the
    // PipeWire→pipewiresrc→AES67 path for lower latency.
    //
    // For each active send instance, creates:
    //   queue → audioconvert → S24BE,48kHz,Nch caps → rtpL24pay → udpsink
    // and links it as a new branch on the tee.
    //
    // Returns a list of GstElement* branch entry queues (one per send instance).
    // The caller must call DetachInlineRTPBranches() before destroying the pipeline.
    struct InlineRTPBranch {
        int instanceId;
        GstElement* queue = nullptr;     // branch entry point (added to caller's bin)
        GstPad* teeSrcPad = nullptr;     // requested pad on tee (must be released)
    };
    std::vector<InlineRTPBranch> AttachInlineRTPBranches(GstElement* pipeline, GstElement* tee);

    // Detach and clean up inline RTP branches attached by AttachInlineRTPBranches.
    void DetachInlineRTPBranches(GstElement* pipeline, std::vector<InlineRTPBranch>& branches);

    // Pause/resume standalone send pipelines.
    // Called by GStreamerOutput so that send pipelines only run PLAYING
    // while media is actually being played, preventing digital noise on
    // the AES67 output when nothing is playing (pipewiresrc would capture
    // uninitialized data from the idle PipeWire node).
    void PauseSendPipelines();
    void ResumeSendPipelines();
    void FlushSendPipelines();

    AES67Manager();
    virtual ~AES67Manager();

private:

    // Config
    AES67Config m_config;
    std::string m_configPath;
    bool LoadConfig();

    // Guards every access to m_config.
    //
    // LoadConfig() clears and refills m_config.instances, which reallocates
    // the vector and reassigns its std::strings.  ApplyConfig() and the SAP
    // threads are safe (ApplyConfig joins those threads before loading), but
    // GetStatus(), RunSelfTest() and the PTP query helpers all run on HTTP or
    // command threads and read m_config concurrently -- iterating a vector
    // that is being reallocated, or reading a std::string that is being
    // reassigned, is a use-after-free.  UIs poll status, so this is the
    // likeliest crash in the field.
    //
    // Held only for the duration of a read or the final swap in LoadConfig(),
    // and NEVER together with m_pipelineMutex -- readers snapshot what they
    // need, release, then take the pipeline lock.  That keeps the two locks
    // unordered with respect to each other and makes deadlock impossible.
    std::mutex m_configMutex;

    // Snapshot accessors -- copy under the lock, use the copy afterwards.
    AES67Config GetConfigSnapshot();
    bool IsPtpEnabled();
    int GetPtpDomain();
    std::string GetPtpInterface();

    // PTP (managed via ptp4l subprocess)
    bool m_ptpInitialized = false;
    pid_t m_ptp4lPid = -1;               // ptp4l child process ID
    pid_t m_phc2sysPid = -1;             // phc2sys child process ID
    std::string m_ptpConfPath;           // temp config file for ptp4l
    bool InitPTP();
    void ShutdownPTP();
    bool IsPtp4lRunning() const;         // check if ptp4l process is alive
    std::string GetPTPClockId();         // EUI-64 from interface MAC (this node's own identity)
    std::string GetPtp4lState();         // query ptp4l port state via pmc (MASTER/SLAVE/LISTENING/...)

    // Write an AES67-profile ptp4l config.  Split out so the three start
    // attempts (hardware, software, software-without-DSCP) cannot drift apart
    // -- they used to be copy-pasted blocks.
    bool WritePtpConf(const std::string& path, bool hwTimestamping, bool includeDscp);

    // fork/exec ptp4l with the given config.  Returns true if it is still
    // alive shortly afterwards.
    bool StartPtp4l(bool hwTimestamping, bool includeDscp);

    // Restart ptp4l/phc2sys if they have died (link flap, OOM, manual kill).
    // Called from the SAP threads alongside the pipeline watchdog.
    void CheckPtpWatchdog();

    // Query the actual PTP grandmaster (may be a remote clock, not this node)
    // via `pmc GET TIME_STATUS_NP`.  Returns false if ptp4l isn't running or
    // the query fails, leaving the out-params untouched.
    bool QueryPtp4lTimeStatus(bool& gmPresent, std::string& gmIdentity, int64_t& offsetNs);

    // The AES67 media clock -- PTP time, shared by every send pipeline so they
    // all sit on one timeline.  Created on first use, released in Shutdown().
    // Null when PTP is disabled or no PTP time source could be opened, in
    // which case pipelines fall back to GStreamer's default clock.
    GstClock* m_ptpClock = nullptr;
    std::mutex m_ptpClockMutex;
    GstClock* GetOrCreateMediaClock();
    void ReleaseMediaClock();

    // Grandmaster identity to advertise in SDP / report over HTTP: the remote
    // clock we follow, or our own identity when we hold the role.  Empty when
    // PTP is enabled but no grandmaster has been settled on yet.
    std::string GetActiveGrandmasterId();

    // Cached pmc results -- see AES67::PTP_QUERY_CACHE_MS.
    struct PtpQueryCache {
        std::chrono::steady_clock::time_point when{};
        bool valid = false;
        bool gmPresent = false;
        std::string gmIdentity;
        int64_t offsetNs = 0;
        std::string portState;
    };
    PtpQueryCache m_ptpCache;
    std::mutex m_ptpCacheMutex;
    void RefreshPtpCache(bool force = false);

    // Sink-pacing timestamp shift per instance, updatable once the pipeline
    // reports its real latency.  Owned here so the pad probe can read a live
    // value rather than one baked in before the latency is known.
    std::map<int, std::atomic<GstClockTime>*> m_sinkPacingShift;

    // Drift control loop -- see AES67Config::adaptiveResample.  Runs on its own
    // thread because it has to sample far more often than the 30s watchdog.
    std::thread m_driftThread;
    std::atomic<bool> m_driftRunning{false};
    void DriftControlLoop();

    // Pipeline watchdog — called from SAP thread to poll bus messages
    // and restart any pipeline that isn't in PLAYING state.
    // Returns true if a full pipeline rebuild is needed.
    bool PollPipelinesWatchdog();

    // Deferred pipeline-rebuild thread — spawned by SAPAnnounceLoop()'s
    // watchdog rebuild path to call ApplyConfig() from a thread other than
    // the one ApplyConfig() needs to join (m_sapAnnounceThread).  Tracked
    // as a member (rather than detached) so Shutdown() can join it before
    // tearing anything else down -- see Shutdown() and SAPAnnounceLoop().
    std::thread m_rebuildThread;

    // Serializes the whole tear-down/rebuild sequence in ApplyConfig(),
    // Shutdown() and Cleanup().  ApplyConfig() joins the SAP threads at the
    // top and re-creates them ~70 lines later, after loading config and
    // building pipelines; that window is wide, and ApplyConfig() has three
    // independent callers (the boot sequence, AES67ApplyCommand::run on a
    // command thread, and m_rebuildThread from the watchdog).  Two overlapping
    // calls both pass the join, then both assign m_sapAnnounceThread -- and
    // assigning over a still-joinable std::thread is an unconditional
    // std::terminate().
    //
    // Shutdown() must join m_rebuildThread BEFORE taking this lock: that
    // thread calls ApplyConfig(), so holding the lock across the join would
    // deadlock.
    std::mutex m_applyMutex;

    // Pipeline management
    std::map<int, AES67Pipeline> m_sendPipelines;    // keyed by instance ID
    std::map<int, AES67Pipeline> m_recvPipelines;
    std::mutex m_pipelineMutex;

    bool CreateSendPipeline(const AES67Instance& inst);
    bool CreateRecvPipeline(const AES67Instance& inst);
    void StopPipeline(AES67Pipeline& p);
    void StopAllPipelines();

    // Get source IP for the network interface
    std::string GetInterfaceIP(const std::string& iface);

    // SAP announcer thread
    std::thread m_sapAnnounceThread;
    std::atomic<bool> m_sapAnnounceRunning{false};
    void SAPAnnounceLoop();
    std::string BuildSDP(const AES67Instance& inst, const std::string& sourceIP,
                         const std::string& ptpClockId, uint32_t sdpVersion);
    std::vector<uint8_t> BuildSAPPacket(const std::string& sourceIP,
                                        uint16_t msgIdHash,
                                        const std::string& sdp,
                                        bool isDeletion = false);

    // RFC 2974 requires the message id hash to change whenever the announced
    // payload changes, so it is computed over the SDP text itself rather than
    // over the instance -- otherwise receivers dedupe an updated announcement
    // as a repeat and never see the corrected refclk.
    uint16_t ComputeSAPHash(const std::string& sdp);

    // SDP o= version for a given announcement body.  Only advances when the
    // announced content actually changes: the msg id hash is derived from the
    // SDP text, so bumping the version on every rebuild would mint a new hash
    // each time and leave SAP receivers listing a phantom stream per restart
    // until their timeout (~10 announce intervals) cleared it.
    //
    // The mapping is persisted so an fppd restart with unchanged config
    // re-announces byte-identical SDP rather than a new session.
    uint32_t SDPVersionFor(const std::string& body);
    void LoadSDPVersion();
    void SaveSDPVersion();
    std::string m_sdpVersionPath;
    std::string m_sdpBodyKey;    // hash of the last announced body
    uint32_t NextSDPVersion();
    // Bumped from the SAP thread on every rebuild and from RunSelfTest() on
    // whichever thread asked for the test, so it cannot be a plain int.
    std::atomic<uint32_t> m_lastSdpVersion{0};

    // SAP receiver thread
    std::thread m_sapRecvThread;
    std::atomic<bool> m_sapRecvRunning{false};
    void SAPReceiveLoop();
    std::map<uint16_t, SAPDiscoveredStream> m_discoveredStreams;
    std::mutex m_discoveredMutex;
    void HandleSAPPacket(const uint8_t* data, size_t len,
                         const std::string& senderAddr);

    // GStreamer bus callback
    static gboolean OnBusMessage(GstBus* bus, GstMessage* msg, gpointer userData);

    // State
    std::atomic<bool> m_active{false};
    std::atomic<bool> m_initialized{false};

    // Safe node name (matches fpp_aes67_common.py safe_node_name)
    static std::string SafeNodeName(const std::string& name);
};

#endif // HAS_AES67_GSTREAMER

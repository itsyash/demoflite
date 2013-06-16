/*************************************************************************/
/*                                                                       */
/*                  Language Technologies Institute                      */
/*                     Carnegie Mellon University                        */
/*                         Copyright (c) 2010                            */
/*                        All Rights Reserved.                           */
/*                                                                       */
/*  Permission is hereby granted, free of charge, to use and distribute  */
/*  this software and its documentation without restriction, including   */
/*  without limitation the rights to use, copy, modify, merge, publish,  */
/*  distribute, sublicense, and/or sell copies of this work, and to      */
/*  permit persons to whom this work is furnished to do so, subject to   */
/*  the following conditions:                                            */
/*   1. The code must retain the above copyright notice, this list of    */
/*      conditions and the following disclaimer.                         */
/*   2. Any modifications must be clearly marked as such.                */
/*   3. Original authors' names are not deleted.                         */
/*   4. The authors' names are not used to endorse or promote products   */
/*      derived from this software without specific prior written        */
/*      permission.                                                      */
/*                                                                       */
/*  CARNEGIE MELLON UNIVERSITY AND THE CONTRIBUTORS TO THIS WORK         */
/*  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      */
/*  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   */
/*  SHALL CARNEGIE MELLON UNIVERSITY NOR THE CONTRIBUTORS BE LIABLE      */
/*  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    */
/*  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   */
/*  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          */
/*  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       */
/*  THIS SOFTWARE.                                                       */
/*                                                                       */
/*************************************************************************/
/*             Author:  Alok Parlikar (aup@cs.cmu.edu)                   */
/*               Date:  March 2010                                       */
/*************************************************************************/
/*                                                                       */
/*  Interface to the eyes-free project for Flite (www.cmuflite.org)      */
/*                                                                       */
/*************************************************************************/

// Standard headers
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <vector>
#include <iostream> 
#include <sstream>
// Flite headers
#include <flite.h>
#include <jni.h>
// Local headers
#include "./edu_cmu_cs_speech_tts_Common.h"
#include "./edu_cmu_cs_speech_tts_fliteVoices.h"
#include "./edu_cmu_cs_speech_tts_String.h"
//#include "./edu_cmu_us_speech_tts_wordcallback.h"

using namespace std;

namespace FliteEngine {
class Utterance {
public:
	String getText();
	String getID();
	virtual ~Utterance();
	vector<float> word_durations;
	vector<int> word_indices;
	vector<float> seg_durations;
	vector<String> seg_labels;
	int cur_index;
	int cur_seg_index;

private:
	String id_;
	String text_;
};
}

namespace FliteEngine {

Utterance::~Utterance() {
}

}
static android_tts_synth_cb_t ttsSynthDoneCBPointer;
static int ttsAbort = 0;
static int ttsStream = 1;
static int callback = 0;
char* flite_voxdir_path;

FliteEngine::Voices* loadedVoices;
FliteEngine::Voice* currentVoice;
FliteEngine::Utterance* utt1 = new FliteEngine::Utterance();
JavaVM *jvm; //declared by me
JNIEnv *myenv = 0; //declared by me
JNIEnv* env1;
jobject obj;
using namespace FliteEngine;
//utt1 = new Utterance();
/* BEGIN VOICE SPECIFIC CODE */

// Declarations
extern "C" void usenglish_init(cst_voice *v);
extern "C" cst_lexicon *cmulex_init(void);
#ifdef __cplusplus
extern "C" {
#endif

void setVoiceList() {
	if (loadedVoices != NULL) {
		LOGW("Voices already initialized!");
		return;
	}
	LOGI("Starting setVoiceList");
	flite_add_lang("eng", usenglish_init, cmulex_init);
	loadedVoices = new FliteEngine::Voices(0,
			FliteEngine::ONLY_ONE_VOICE_REGISTERED); // Max number of voices is the first argument.
	if (loadedVoices == NULL) {
		LOGE("Voice list could not be initialized!");
		return;
	}
	LOGI("setVoiceList: list initialized");
	LOGI("setVoiceList done");
}

/* END VOICE SPECIFIC CODE */

#define sgn(x) (x>0?1:x?-1:0)

void compress(short *samples, int num_samples, float mu) {
	int i = 0;
	short limit = 30000;
	short x;
	for (i = 0; i < num_samples; i++) {
		x = samples[i];
		samples[i] = limit
				* (sgn(x) * (log(1 + (mu / limit) * abs(x)) / log(1 + mu)));
	}
}

void callbackhelper(int index) {
	LOGI("in call back %d", index);

	return;
}

/* Callback from flite. Should call back the TTS API */
static int fliteCallback(const cst_wave *w, int start, int size, int last,
		cst_audio_streaming_info_struct *asi) {//from where are we getting this callback

	int isword = -1;
	int isattached = 0;
		if (start == 0) {
		utt1 = new FliteEngine::Utterance();
	}
	short *waveSamples = (short *) &w->samples[start];
	compress(waveSamples, size, 5);
	//LOGV("Compressing with 5");

	int8_t *castedWave = (int8_t *) &w->samples[start];
	size_t bufferSize = size * sizeof(short);
	int num_channels = w->num_channels;
	int sample_rate = w->sample_rate;

	LOGV("flite callback received! Start: %d. Size: %d. Last: %d. Channels: %d.", start, size, last, num_channels);
	// Flite* fte = (Flite*) asi->userdata;
	Utterance* utt = utt1;
	int utt_callback_stop_requested = 0;
//LOGI("im here safe 2");

	if (1) {//word level starts here

		// If this is the first callback, we need to parse the utterance and
		// get valuable duration information
		LOGI("im here safe 3 isempty:%d. size:%d.",utt->word_durations.empty(),utt->word_durations.size());
		//LOGI("result %d",utt->getID());
		if (utt->word_durations.empty()) {
//LOGI("im here safe 4");
			utt->cur_index = 0;
			const cst_utterance* u = asi->utt;
			String token, old_token, token_name;
			float word_start_time = 0;

			float word_end_time = 0;

			int idx_count = 0;

			cst_item* item;
//LOGI("im here safe 5 ");
			for (item = relation_head(utt_relation(u, "Token")); item; item = item_next(item)) {
				word_end_time =ffeature_float(item,"p.daughtern.R:SylStructure.daughtern.daughtern.R:Segment.end");
				if (word_end_time == 0) {
					// This token can't be synthesized, so put it together with the previous one if exists. otherwise, store as zero.
					if (!utt->word_durations.empty())
						word_end_time = utt->word_durations.back();
				}

				idx_count = ffeature_int(item, "file_pos");
				utt->word_durations.push_back(word_end_time);
				utt->word_indices.push_back(idx_count);
			}
		}

		float start_time = float(start) / w->sample_rate;
		float end_time = float(start + size) / w->sample_rate;
//	String utt_id = "c";//utt->getID();

		int cur_index = utt->cur_index; // Local copy
		vector<float>::iterator durations = utt->word_durations.begin();
		vector<int>::iterator indices = utt->word_indices.begin();
		int num_words = utt->word_durations.size();
		LOGI("no of words :%d", (num_words));
		// Start event
		if (start == 0)
			// STREAM_STATUS_ERR(fte->utt_event_callback_->sendEvent(utt_id, START_EVENT, 0, 0), utt_callback_stop_requested);
			callbackhelper(0);
		// Word events
		LOGI("cond1:%d. cond2:%d. cond3:%d.",( cur_index < num_words ),( *(durations+cur_index) >= start_time ),( *(durations+cur_index) <  end_time));
	while ((cur_index < num_words) && (*(durations + cur_index) >= start_time) && (*(durations + cur_index) < end_time)) {
			callbackhelper(*(indices + cur_index));
			isword = cur_index;
			LOGI("word level call back is here1 %d",*(indices+cur_index));
			LOGI("with word_no:%d",(cur_index));
			//ttslistener->onWordCompleted(*(indices+cur_index));

			//    STREAM_STATUS_ERR(fte->utt_event_callback_->sendEvent(utt_id, WORD_EVENT,
			//							  *(indices+cur_index),
			//							  *(durations+cur_index)),
			//		      utt_callback_stop_requested);
			cur_index++;
		}

		// If we are at the last streaming call, we want to be sure we
		// call back all remainder words. Ideally we should never get in
		// this position, but could happen once in a while
		if (last == 1) {
			// Catch errors
			if (cur_index != num_words) {
				LOGI("WARNING: Word durations in Utterance do not match with synthesized waveform.");

				while (cur_index < num_words) {

					callbackhelper(*(indices + cur_index));
					LOGI("word level call back is here2 %d",*(indices+cur_index));
					//ttslistener->onWordCompleted(*(indices+cur_index));

					// Word events
					/* STREAM_STATUS_ERR(fte->utt_event_callback_->sendEvent(utt_id, WORD_EVENT,
					 *(indices+cur_index),
					 *(durations+cur_index)),
					 utt_callback_stop_requested);*/
					cur_index++;
				}
			}

			callbackhelper(-1);
			LOGI("word level call back is done here");
			// Send end event
			//ttslistener->onDone();
			/* STREAM_STATUS_ERR(fte->utt_event_callback_->sendEvent(utt_id, END_EVENT,
			 utt->getText().length(),
			 end_time),
			 utt_callback_stop_requested);*/
		}
		utt->cur_index = cur_index; // Save updates to the object variable

		if (utt_callback_stop_requested) {
			return CST_AUDIO_STREAM_STOP;
		}
	}

	/*end*/

	if (ttsSynthDoneCBPointer != NULL) {
		if (last == 1) {


			/* Bug in audio rendering: Short utterances are not played. Fix it by playing silence in addition. */
			float dur = (start + size) / sample_rate;
			if (dur < 0.8) {
				// create padding
				size_t padding_length = num_channels * (sample_rate / 2);
				int8_t* paddingWave = new int8_t[padding_length]; // Half a second
				for (int i = 0; i < (int) padding_length; i++)
					paddingWave[i] = 0;
				LOGE("Utterance too short. Adding padding to the output to workaround audio rendering bug.");
				ttsSynthDoneCBPointer(&asi->userdata, sample_rate,
						ANDROID_TTS_AUDIO_FORMAT_PCM_16_BIT, num_channels,
						&castedWave, &bufferSize, ANDROID_TTS_SYNTH_PENDING,isword);
				ttsSynthDoneCBPointer(&asi->userdata, sample_rate,
						ANDROID_TTS_AUDIO_FORMAT_PCM_16_BIT, num_channels,
						&paddingWave, &padding_length, ANDROID_TTS_SYNTH_DONE,-2);
				delete[] paddingWave;
			} else
				ttsSynthDoneCBPointer(&asi->userdata, sample_rate,
						ANDROID_TTS_AUDIO_FORMAT_PCM_16_BIT, num_channels,
						&castedWave, &bufferSize, ANDROID_TTS_SYNTH_DONE,isword);
		} else{

			ttsSynthDoneCBPointer(&asi->userdata, sample_rate,
					ANDROID_TTS_AUDIO_FORMAT_PCM_16_BIT, num_channels,
					&castedWave, &bufferSize, ANDROID_TTS_SYNTH_PENDING,isword);

		//	LOGV("flite callback processed!");
		}
	} else {
		LOGE("flite callback not processed because it's NULL!");
		ttsAbort = 1;
	}
	//jvm->DetachCurrentThread();
	if (ttsAbort == 1)
		return CST_AUDIO_STREAM_STOP;

	return CST_AUDIO_STREAM_CONT;
}

// Initializes the TTS engine and returns whether initialization succeeded
android_tts_result_t init(void* engine, android_tts_synth_cb_t synthDoneCBPtr,
		const char *engineConfig) {
	LOGI("TtsEngine::init start");

	LOGI("Compilation Build Date: %s %s", __DATE__, __TIME__);
	LOGI("Compilation Platform: %s", ANDROID_BUILD_ABI);

	// First make sure we receive the data directory. That's very crucial.
	if ((engineConfig != NULL) && (strlen(engineConfig) > 0)) {
		flite_voxdir_path = reinterpret_cast<char*>(malloc(
				strlen(engineConfig) + 12));
		snprintf(flite_voxdir_path, strlen(engineConfig) + 12, "%s/%s",
				engineConfig, "flite-data");
	} else {
		LOGE("External storage directory not specified in engineConfig. ERROR.");
		LOGE("TtsEngine::init fail");
		return ANDROID_TTS_FAILURE;
	}
	//synthDoneCBPtr is the name of the function to which callback will be done
	ttsSynthDoneCBPointer = synthDoneCBPtr; //ttsSynthDoneCBPointer is used to call back to the Service.cpp
	flite_init();
	setVoiceList();
	if (loadedVoices == NULL) {
		LOGE("TTSEngine::init Could not load voice list");
		return ANDROID_TTS_FAILURE;
	}
	currentVoice = loadedVoices->getCurrentVoice();
	if (currentVoice == NULL) {
		LOGE("TTSEngine::init Voice list error");
		return ANDROID_TTS_FAILURE;
	}
	if (currentVoice->getFliteVoice() == NULL) {
		return ANDROID_TTS_FAILURE;
	}
	ttsAbort = 0;
	LOGI("TtsEngine::init done");
	return ANDROID_TTS_SUCCESS;
}

// Shutsdown the TTS engine. Unload all voices
android_tts_result_t shutdown(void* engine) {
	if (flite_voxdir_path != NULL) {
		free(flite_voxdir_path);
	}

	LOGI("TtsEngine::shutdown");
	if (loadedVoices != NULL)
		delete loadedVoices;
	loadedVoices = NULL;
	return ANDROID_TTS_SUCCESS;
}

// We load language when we set the language. Thus, this function always succeeds.
android_tts_result_t loadLanguage(void* engine, const char *lang,
		const char *country, const char *variant) {
	LOGI(
			"TtsEngine::loadLanguage: lang=%s, country=%s, variant=%s", lang, country, variant);
	return ANDROID_TTS_SUCCESS;
}
android_tts_result_t setcallback(void* engine) {
	callback = 1;
	return ANDROID_TTS_SUCCESS;
}

// Set the language based on locale. We use our voices manager to do this job.
android_tts_result_t setLanguage(void* engine, const char * lang,
		const char * country, const char * variant) {
	LOGI(
			"TtsEngine::setLanguage: lang=%s, country=%s, variant=%s", lang, country, variant);

	// Request the voice to voice-manager
	currentVoice = loadedVoices->getVoiceForLocale(lang, country, variant);
	if (currentVoice == NULL) {
		LOGE("TtsEngine::setLanguage : Could not set voice");
		return ANDROID_TTS_FAILURE;
	}
	// Request the voice to voice-manager
	currentVoice = loadedVoices->getVoiceForLocale(lang, country, variant);
	if (currentVoice == NULL) {
		LOGE("TtsEngine::setLanguage : Could not set voice");
		return ANDROID_TTS_FAILURE;
	}

	if (currentVoice->getFliteVoice() == NULL)
		return ANDROID_TTS_FAILURE;
	else
		return ANDROID_TTS_SUCCESS;
}

// Language availability check does not use the "streaming" byte, as in setLanguage
// Also, check is made against the entire locale.
android_tts_support_result_t isLanguageAvailable(void* engine, const char *lang,
		const char *country, const char *variant) {
	LOGI(
			"TtsEngine::isLanguageAvailable: lang=%s, country=%s, variant=%s", lang, country, variant);

	// The hack to set streaming:
	// If language and country are not set, then variant will be
	// interpreted as being "stream" or "nostream" to set the appropriate parameters.
	// The default is to stream.
	if ((strcmp(lang, "") == 0) && (strcmp(country, "") == 0)) {
		if (strcmp(variant, "stream") == 0) {
			LOGI(
					"Streaming setting hack: TTS Streaming is ENABLED. Synthesis Benchmarks DISABLED.");
			ttsStream = 1;
			return ANDROID_TTS_LANG_COUNTRY_VAR_AVAILABLE;
		} else if (strcmp(variant, "nostream") == 0) {
			LOGI(
					"Streaming setting hack: TTS Streaming is DISABLED. Synthesis Benchmarks ENABLED.");
			ttsStream = 0;
			return ANDROID_TTS_LANG_COUNTRY_VAR_AVAILABLE;
		} else {
			LOGE(
					"Streaming setting hack: Incorrect setting %s. If you don't specify language and country, variant should be 'stream' or 'nostream'", variant);
			return ANDROID_TTS_LANG_NOT_SUPPORTED;
		}
	}

	return loadedVoices->isLocaleAvailable(lang, country, variant);
}

// Provide information about the currently set language.
android_tts_result_t getLanguage(void* engine, char *language, char *country,
		char *variant) {
	LOGI("TtsEngine::getLanguage");
	if (currentVoice == NULL)
		return ANDROID_TTS_FAILURE;

	strcpy(language, currentVoice->getLanguage());
	strcpy(country, currentVoice->getCountry());
	strcpy(variant, currentVoice->getVariant());
	return ANDROID_TTS_SUCCESS;
}

// Setting Audio Format is not supported by Flite Engine.
android_tts_result_t setAudioFormat(void* engine,
		android_tts_audio_format_t* encoding, uint32_t* rate, int* channels) {
	LOGI("TtsEngine::setAudioFormat");
	cst_voice* flite_voice;
	if (currentVoice == NULL) {
		LOGE("Voices not loaded?");
		return ANDROID_TTS_FAILURE;
	}
	flite_voice = currentVoice->getFliteVoice();
	if (flite_voice == NULL) {
		LOGE("Voice not available");
		return ANDROID_TTS_FAILURE;
	}

	*rate = feat_int(flite_voice->features, "sample_rate");
	LOGI("TtsEngine::setAudioFormat: setting Rate to %d", rate);

	*encoding = ANDROID_TTS_AUDIO_FORMAT_PCM_16_BIT;
	*channels = 1;

	return ANDROID_TTS_FAILURE;
}

// Properties are not yet implemented.
android_tts_result_t setProperty(void* engine, const char *property,
		const char *value, const size_t size) {
	LOGW("TtsEngine::setProperty is unimplemented");
	return ANDROID_TTS_PROPERTY_UNSUPPORTED;
	// Other values hint: TTS_VALUE_INVALID, TTS_SUCCESS
}

//Properties are not yet implemented.
android_tts_result_t getProperty(void* engine, const char *property,
		char *value, size_t *iosize) {
	LOGW("TtsEngine::getProperty is unimplemented");
	return ANDROID_TTS_PROPERTY_UNSUPPORTED;
}

// Support for synthesizing IPA text is not implemented.
android_tts_result_t synthesizeIpa(void* engine, const char * ipa,
		int8_t * buffer, size_t bufferSize, void * userdata) {
	LOGI("TtsEngine::synthesizeIpa");
	return ANDROID_TTS_FAILURE;
}

// Interrupts synthesis.
android_tts_result_t stop(void* engine) {
	LOGI("TtsEngine::stop");
	ttsAbort = 1;
	return ANDROID_TTS_SUCCESS;
}

android_tts_result_t setjnienv(void* engine, JNIEnv* env, jobject object) {
	LOGI("TtsEngine::set jni successfull");
	env1 = env;
	obj = object;
	if (env->GetJavaVM(&jvm) == 0) {
		LOGV("jvm is fine");
	}
	return ANDROID_TTS_SUCCESS;
}

// Synthesize Text. Check if streaming is requested, and stream if so.
android_tts_result_t synthesizeText(void* engine, const char * text,
		int8_t * buffer, size_t bufferSize, void * userdata) {
	cst_voice* flite_voice;
	if (currentVoice == NULL) {
		LOGE("Voices not loaded?");
		return ANDROID_TTS_FAILURE;
	}
	flite_voice = currentVoice->getFliteVoice();
	if (flite_voice == NULL) {
		LOGE("Voice not available");
		return ANDROID_TTS_FAILURE;
	}

	LOGI(
			"TtsEngine::synthesizeText: text=%s, bufferSize=%lu", text, (unsigned long) bufferSize);

	if (ttsStream) {
		LOGI("TtsEngine::synthesizeText: streaming is ENABLED");
		ttsAbort = 0;
		cst_audio_streaming_info *asi;// defined in flite/include/cst_audio.h
		asi = new_audio_streaming_info();
		asi->min_buffsize = bufferSize;
		asi->asc = fliteCallback;
		asi->userdata = userdata;
		feat_set(flite_voice->features, "streaming_info",
				audio_streaming_info_val(asi));
		LOGI("TtsEngine::synthesizeText: after feat_set");
		/* SSML support */
		if (!strncmp(text, "<?xml version=\"1.0\"?>", 21)) {
			LOGE("TtsEngine: Using SSML mode for %s", text);
			// XML string given. Parse as SSML.

			// For now, just strip off all tags and say whatever remains.
			int pos1 = 0, pos2 = 0;
			int maxpos = strlen(text);

			char* textOverride = (char*) text;

			while (pos2 < maxpos) {
				while ((pos2 < maxpos) && (textOverride[pos2] != '<')) {
					textOverride[pos1] = textOverride[pos2];
					pos1++;
					pos2++;
				}
				while ((pos2 < maxpos) && (textOverride[pos2] != '>')) {
					pos2++;
				}
			}
			textOverride[pos1] = '\0';

		}

		cst_utterance *u = flite_synth_text(text, flite_voice);//defined in flite/src/synth/flite.c
		LOGI("TtsEngine::synthesizeText: after flite_synth_text");
		delete_utterance(u);

		feat_remove(flite_voice->features, "streaming_info");

		LOGI("Done flite synthesis.");

		return ANDROID_TTS_SUCCESS;
	} else {
		LOGI("TtsEngine::synthesizeText: streaming is DISABLED");
		LOGI("Starting Synthesis");

		timespec start, end;
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start);

		cst_wave* w = flite_text_to_wave(text, flite_voice);

		compress(w->samples, w->num_samples, 5);
		LOGV("Compressing with 5");

		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end);

		// Calculate time difference
		timespec temp;
		if ((end.tv_nsec - start.tv_nsec) < 0) {
			temp.tv_sec = end.tv_sec - start.tv_sec - 1;
			temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
		} else {
			temp.tv_sec = end.tv_sec - start.tv_sec;
			temp.tv_nsec = end.tv_nsec - start.tv_nsec;
		}

		float diffmilliseconds = 1000 * temp.tv_sec + (temp.tv_nsec) / 1000000;

		float wavlen = 1000 * ((float) w->num_samples / w->sample_rate);
		float timesrealtime = wavlen / diffmilliseconds;
		LOGW(
				"A %1.2f ms file synthesized in %1.2f ms: synthesis is %1.2f times faster than real time.", wavlen, diffmilliseconds, timesrealtime);

		LOGI("Done flite synthesis.");

		size_t bufSize = w->num_samples * sizeof(short);
		int8_t* castedWave = (int8_t *) w->samples;

		if (ttsSynthDoneCBPointer != NULL)
			ttsSynthDoneCBPointer(&userdata, w->sample_rate,
					ANDROID_TTS_AUDIO_FORMAT_PCM_16_BIT, w->num_channels,
					&castedWave, &bufSize, ANDROID_TTS_SYNTH_DONE,-2);
		else {
			LOGI("flite callback not processed because it's NULL!");
		}

		delete_wave(w);

		return ANDROID_TTS_SUCCESS;
	}
}

// Function to get TTS Engine
android_tts_engine_t *getTtsEngine() {
	LOGI("TtsEngine::getTtsEngine");

	android_tts_engine_t* engine;
	engine = (android_tts_engine_t*) malloc(sizeof(android_tts_engine_t));
	android_tts_engine_funcs_t* functable =
			(android_tts_engine_funcs_t*) malloc(
					sizeof(android_tts_engine_funcs_t));
	functable->init = &init;
	functable->shutdown = &shutdown;
	functable->stop = &stop;
	functable->isLanguageAvailable = &isLanguageAvailable;
	functable->loadLanguage = &loadLanguage;
	functable->setLanguage = &setLanguage;
	functable->getLanguage = &getLanguage;
	functable->setAudioFormat = &setAudioFormat;
	functable->setProperty = &setProperty;
	functable->getProperty = &getProperty;
	functable->synthesizeText = &synthesizeText;
	functable->setjnienv = &setjnienv;
	functable->setcallback = &setcallback;
	engine->funcs = functable;
	return engine;
}
android_tts_engine_t *android_getTtsEngine() {
	return getTtsEngine();
}

// This function generates a benchmark of how fast the current voice is.
float getBenchmark() {
	cst_voice* flite_voice;
	if (currentVoice == NULL) {
		LOGE("Voices not loaded?");
		return -1;
	}

	flite_voice = currentVoice->getFliteVoice();
	if (flite_voice == NULL) {
		LOGE("Voice not available");
		return -1;
	}

	LOGI("TtsEngine Running Benchmark");

	timespec start, end;
	float totalmilliseconds = 0;
	float wavlen;
	int num_trials = 10;

	const char* text =
			"This sentence is the being synthesized for a benchmark computation.";

	for (int i = 0; i < num_trials; i++) {
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start);
		cst_wave* w = flite_text_to_wave(text, flite_voice);
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end);

		// Calculate time difference
		timespec temp;
		if ((end.tv_nsec - start.tv_nsec) < 0) {
			temp.tv_sec = end.tv_sec - start.tv_sec - 1;
			temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
		} else {
			temp.tv_sec = end.tv_sec - start.tv_sec;
			temp.tv_nsec = end.tv_nsec - start.tv_nsec;
		}

		float diffmilliseconds = 1000 * temp.tv_sec + (temp.tv_nsec) / 1000000;
		totalmilliseconds += diffmilliseconds;
		wavlen = 1000 * ((float) w->num_samples / w->sample_rate);
		delete_wave(w);
	}

	float timesrealtime = (wavlen * num_trials) / totalmilliseconds;
	return timesrealtime;
}
#ifdef __cplusplus
}
#endif

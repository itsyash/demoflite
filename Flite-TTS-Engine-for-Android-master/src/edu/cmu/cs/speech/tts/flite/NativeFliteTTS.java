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
/*               Date:  June 2012                                        */
/*************************************************************************/

package edu.cmu.cs.speech.tts.flite;

import java.io.File;

import android.content.Context;
import android.os.Environment;
import android.util.Log;
import android.widget.Toast;


public class NativeFliteTTS {
	private final static String LOG_TAG = "Flite_Java_" + NativeFliteTTS.class.getSimpleName();	
	
	public interface OnWordCompletedListener
	{
	public  void onWordCompleted(int startPosition);
	public  void onDone();
	}
	static {
		System.loadLibrary("ttsflite");
		nativeClassInit();
	}

	private OnWordCompletedListener listen;
	private final Context mContext;
	private final SynthReadyCallback mCallback;
    private final String mDatapath;
    private boolean mInitialized = false;
    
	public NativeFliteTTS(Context context, SynthReadyCallback callback) {
		mDatapath = new File(Voice.getDataStorageBasePath()).getParent();
		mContext = context;
		mCallback = callback;
		attemptInit();		
	}
	
	@Override
    protected void finalize() {
        nativeDestroy();
    }
	
	public int isLanguageAvailable(String language, String country,	String variant) {
		return nativeIsLanguageAvailable(language, country, variant);
	}
	
	public boolean setLanguage(String language, String country, String variant) {
        attemptInit();
        return nativeSetLanguage(language, country, variant);
    }
	
	public void synthesize(String text) {
        nativeSynthesize(text);
    }

    public void stop() {
        nativeStop();
    }
    
    public String getNativeABI() {
    	return nativeGetABI();
    }
    
    public float getNativeBenchmark() {
		return nativeGetBenchmark();
	}
    public void gettest()
    {
    	Log.v(LOG_TAG, "in java get test");
    		
    	boolean hey=nativeGetTest();
    }
    public boolean setOnWordCompletedListener(OnWordCompletedListener listener)
    {
    listen=listener;
    Log.v(LOG_TAG, "in java setonwordcompleted");
    boolean result=nativeSetCallback();
    return result;
    }
    
    private void nativeSynthCallback(byte[] audioData) {
        if (mCallback == null)
            return;

        if (audioData == null) {
            mCallback.onSynthDataComplete();
        } else {
            mCallback.onSynthDataReady(audioData);
        }
    }
    
    private void attemptInit() {
        if (mInitialized) {
            return;
        }
        
        if (!nativeCreate(mDatapath)) {
			Log.e(LOG_TAG, "Failed to initialize flite library");
			return;
		}
		Log.i(LOG_TAG, "Initialized Flite");
		mInitialized = true;
        
    }
    public String messageMe(String text) {
    	Log.v(LOG_TAG, text+"in java call back");
    	Toast.makeText(mContext,"back in java",Toast.LENGTH_LONG).show();
    	listen.onWordCompleted(5);
	return text;
    }
	
    private int mNativeData;
	private static native final boolean nativeClassInit();
	private native final boolean nativeCreate(String path);
    private native final boolean nativeDestroy();
	private native final int nativeIsLanguageAvailable(String language, String country, String variant);
	private native final boolean nativeSetLanguage(String language, String country, String variant);
	private native final boolean nativeSynthesize(String text);
	private native final boolean nativeStop();
	
	private native final String nativeGetABI();
	private native final float nativeGetBenchmark();
	private native final boolean nativeGetTest();
	private native final boolean nativeSetCallback();
	public interface SynthReadyCallback {
        void onSynthDataReady(byte[] audioData);

        void onSynthDataComplete();
    }

	
}

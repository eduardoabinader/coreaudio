package com.gigantelabs.platform;

import android.media.AudioManager;

// Native registration and token lifetime belong to gigantePlatform. This class
// forwards facts only; it never opens audio, re-requests focus or owns UI policy.
public final class AudioFocusListener implements AudioManager.OnAudioFocusChangeListener
{
    private final long token;

    public AudioFocusListener(long token)
    {
        this.token = token;
    }

    @Override
    public void onAudioFocusChange(int focusChange)
    {
        nativeOnFocusChange(token, focusChange);
    }

    private static native void nativeOnFocusChange(long token, int focusChange);
}

package com.justivo.heavenwaves.permission_text_providers;

public class RecordAudioPermissionTextProvider implements PermissionTextProvider {
    @Override
    public String getDescription(boolean isPermanentlyDeclined)
    {
        if(isPermanentlyDeclined) {
            return "It seems you permanently declined audio record permission. " +
                    "You can go to the app settings to grant it.";
        } else {
            return "This permission is required to record other apps audio" +
                    "and send it over the network.";
        }
    }
}

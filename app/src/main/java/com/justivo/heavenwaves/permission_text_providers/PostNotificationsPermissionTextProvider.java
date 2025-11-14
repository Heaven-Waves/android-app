package com.justivo.heavenwaves.permission_text_providers;

public class PostNotificationsPermissionTextProvider implements PermissionTextProvider {

    @Override
    public String getDescription(boolean isPermanentlyDeclined)
    {
        if(isPermanentlyDeclined) {
            return "It seems you permanently declined post notifications permission. " +
                    "You can go to the app settings to grant it.";
        } else {
            return "This is required to show the status of the streaming / recording.";
        }
    }
}


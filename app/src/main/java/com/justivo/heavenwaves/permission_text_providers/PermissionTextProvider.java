package com.justivo.heavenwaves.permission_text_providers;

public interface PermissionTextProvider {
    public String getDescription(boolean isPermanentlyDeclined);
}


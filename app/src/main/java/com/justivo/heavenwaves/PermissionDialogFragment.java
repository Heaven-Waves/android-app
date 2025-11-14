package com.justivo.heavenwaves;

import android.app.Dialog;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

import com.justivo.heavenwaves.permission_text_providers.PermissionTextProvider;

public class PermissionDialogFragment extends DialogFragment {

    private final boolean isPermanentlyDeclined;
    private final PermissionTextProvider permissionTextProvider;
    private final Runnable onDismiss;
    private final Runnable onContinue;
    private final Runnable onGoToAppSettingsClick;


    public PermissionDialogFragment(Builder builder) {

        this.isPermanentlyDeclined = builder.isPermanentlyDeclined;
        this.permissionTextProvider = builder.permissionTextProvider;
        this.onDismiss = builder.onDismiss;
        this.onContinue = builder.onContinue;
        this.onGoToAppSettingsClick = builder.onGoToAppSettingsClick;
    }

    @NonNull
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        // Use the Builder class for convenient dialog construction.
        AlertDialog.Builder builder = new AlertDialog.Builder(requireActivity());
        String message = permissionTextProvider.getDescription(isPermanentlyDeclined);
        String buttonText = isPermanentlyDeclined ? "Grant Permissions" : "OK";

        builder.setTitle("Permission Required")
                .setMessage(message)
                .setPositiveButton(buttonText, (dialog, id) -> {
                    if(isPermanentlyDeclined) {
                        onGoToAppSettingsClick.run();
                    } else {
                        onContinue.run();
                    }
                })
                .setOnDismissListener(dialog -> onDismiss.run());
        // Create the AlertDialog object and return it.
        return builder.create();
    }

    public static Builder builder() {
        return new Builder();
    }

    public static class Builder {
        private boolean isPermanentlyDeclined;
        private PermissionTextProvider permissionTextProvider;
        private Runnable onDismiss;
        private Runnable onContinue;
        private Runnable onGoToAppSettingsClick;

        public Builder isPermanentlyDeclined(boolean isPermanentlyDeclined) {
            this.isPermanentlyDeclined = isPermanentlyDeclined;
            return this;
        }

        public Builder permissionTextProvider(PermissionTextProvider permissionTextProvider) {
            this.permissionTextProvider = permissionTextProvider;
            return this;
        }

        public Builder onDismiss(Runnable onDismiss) {
            this.onDismiss = onDismiss;
            return this;
        }

        public Builder onContinue(Runnable onContinue) {
            this.onContinue = onContinue;
            return this;
        }

        public Builder onGoToAppSettingsClick(Runnable onGoToAppSettingsClick) {
            this.onGoToAppSettingsClick = onGoToAppSettingsClick;
            return this;
        }

        public PermissionDialogFragment build() {
            return new PermissionDialogFragment(this);
        }
    }
}

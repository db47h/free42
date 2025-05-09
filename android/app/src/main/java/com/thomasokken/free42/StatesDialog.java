/*****************************************************************************
 * Free42 -- an HP-42S calculator simulator
 * Copyright (C) 2004-2025  Thomas Okken
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see http://www.gnu.org/licenses/.
 *****************************************************************************/

package com.thomasokken.free42;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.graphics.Color;
import android.net.Uri;
import android.support.v4.content.FileProvider;
import android.view.View;
import android.view.WindowManager;
import android.widget.AbsListView;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.TextView;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.UnsupportedEncodingException;
import java.net.URLDecoder;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class StatesDialog extends Dialog {
    private TextView currentLabel;
    private ListView statesList;
    private Button switchToButton;
    private String stateDirName;

    private static StatesDialog instance;

    public StatesDialog(Context ctx, String selectedState) {
        super(ctx);
        instance = this;
        setContentView(R.layout.states_dialog);
        getWindow().setLayout(WindowManager.LayoutParams.MATCH_PARENT,
                WindowManager.LayoutParams.MATCH_PARENT);
        currentLabel = (TextView) findViewById(R.id.currentLabel);
        currentLabel.setText("Current: " + Free42Activity.getSelectedState());
        switchToButton = (Button) findViewById(R.id.switchToButton);
        switchToButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                doSwitchTo();
            }
        });
        Button moreButton = (Button) findViewById(R.id.moreButton);
        moreButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                doMore();
            }
        });
        Button doneButton = (Button) findViewById(R.id.doneButton);
        doneButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                doDone();
            }
        });
        statesList = (ListView) findViewById(R.id.statesView);
        statesList.setChoiceMode(AbsListView.CHOICE_MODE_SINGLE);
        stateDirName = ctx.getFilesDir().getAbsolutePath();
        setTitle("States");

        // Make sure a file exists for the current state. This isn't necessarily
        // the case, specifically, right after starting up with a version <= 25
        // state file.
        String currentStateFileName = stateDirName + "/" + Free42Activity.getSelectedState() + ".f42";
        if (!new File(currentStateFileName).exists()) {
            OutputStream os = null;
            try {
                os = new FileOutputStream(currentStateFileName);
                os.write(Free42Activity.FREE42_MAGIC_STR().getBytes("ASCII"));
            } catch (IOException e) {
                // Oh well, we tried
            } finally {
                if (os != null)
                    try {
                        os.close();
                    } catch (IOException e) {}
            }
        }

        List<String> states = getLoadedStates(stateDirName);
        ArrayAdapter<String> adapter = new ArrayAdapter<String>(ctx,
                android.R.layout.simple_list_item_activated_1, android.R.id.text1, states);
        statesList.setAdapter(adapter);
        statesList.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                selectionChanged();
            }
        });
        if (selectedState != null) {
            int sel = states.indexOf(selectedState);
            if (sel != -1)
                statesList.setItemChecked(sel, true);
        }

        // Force initial update
        selectionChanged();
    }

    @SuppressWarnings("unchecked")
    private String getSelectedState() {
        int sel = statesList.getCheckedItemPosition();
        return sel == -1 ? null : ((ArrayAdapter<String>) statesList.getAdapter()).getItem(sel);
    }

    @SuppressWarnings("unchecked")
    private void updateUI(boolean rescan) {
        List<String> states = getLoadedStates(stateDirName);
        ArrayAdapter<String> adapter = (ArrayAdapter<String>) statesList.getAdapter();
        adapter.clear();
        adapter.addAll(states);
        // Clear selection
        statesList.setAdapter(adapter);
    }

    @SuppressWarnings("unchecked")
    private void selectionChanged() {
        ArrayAdapter<String> adapter = (ArrayAdapter<String>) statesList.getAdapter();
        int sel = statesList.getCheckedItemPosition();
        if (sel == -1) {
            switchToButton.setEnabled(false);
            switchToButton.setText("Switch To");
        } else {
            switchToButton.setEnabled(true);
            String selectedState = adapter.getItem(sel);
            if (selectedState.equals(Free42Activity.getSelectedState()))
                switchToButton.setText("Revert");
            else
                switchToButton.setText(("Switch To"));
        }
    }

    private void doSwitchTo() {
        String selectedStateName = getSelectedState();
        if (selectedStateName == null)
            return;
        if (selectedStateName.equals(Free42Activity.getSelectedState()))
            new AlertDialog.Builder(getContext())
                    .setTitle("Confirm Revert")
                    .setMessage("Are you sure you want to revert the state \"" + selectedStateName + "\" to the last version saved?")
                    .setIcon(android.R.drawable.ic_dialog_info)
                    .setPositiveButton("OK", new DialogInterface.OnClickListener() {
                        public void onClick(DialogInterface dialog, int whichButton) {
                            doSwitchTo2();
                        }
                    })
                    .setNegativeButton("Cancel", null).show();
        else
            doSwitchTo2();
    }

    private void doSwitchTo2() {
        String selectedStateName = getSelectedState();
        if (selectedStateName == null)
            return;
        Free42Activity.switchToState(selectedStateName);
        dismiss();
    }

    private void doNew() {
        getStateName("New state name:", "", 0);
    }

    private void doNew2(String newStateName) {
        String newFileName = stateDirName + "/" + newStateName + ".f42";
        if (new File(newFileName).exists()) {
            Free42Activity.showAlert("That name is already in use.");
            return;
        }
        FileOutputStream fos = null;
        try {
            fos = new FileOutputStream(newFileName);
            fos.write(Free42Activity.FREE42_MAGIC_STR().getBytes("ASCII"));
        } catch (IOException e) {
            // Ignore
        } finally {
            if (fos != null)
                try {
                    fos.close();
                } catch (IOException e) {}
        }
        updateUI(true);
    }

    private String makeCopyName(String name) {
        // We're naming duplicates by appending " copy" or " copy NNN" to the name
        // of the original, but if the name of the original already ends with " copy"
        // or " copy NNN", it seems more elegant to continue the sequence rather than
        // add another " copy" suffix.
        String copyName = name;
        int n = 0;
        if (copyName.endsWith(" copy")) {
            copyName = copyName.substring(0, copyName.length() - 5);
            n = 1;
        } else {
            int cpos = copyName.lastIndexOf(" copy ");
            if (cpos != -1) {
                try {
                    int m = Integer.parseInt(copyName.substring(cpos + 6));
                    if (m > 1) {
                        n = m;
                        copyName = copyName.substring(0, cpos);
                    }
                } catch (NumberFormatException e) {}
            }
        }

        String finalName, finalPath;

        while (true) {
            n++;
            if (n == 1)
                finalName = copyName + " copy";
            else
                finalName = copyName + " copy " + n;
            finalPath = stateDirName + "/" + finalName + ".f42";
            if (!new File(finalPath).exists())
                // File does not exist; that means we have a usable name
                break;
        }

        return finalName;
    }

    private void doDuplicate() {
        String selectedStateName = getSelectedState();
        if (selectedStateName == null)
            return;

        String finalName = makeCopyName(selectedStateName);
        String finalPath = stateDirName + "/" + finalName + ".f42";
        if (selectedStateName.equals(Free42Activity.getSelectedState()))
            Free42Activity.saveStateAs(finalPath);
        else {
            String origPath = stateDirName + "/" + selectedStateName + ".f42";
            FileInputStream fis = null;
            FileOutputStream fos = null;
            try {
                fis = new FileInputStream(origPath);
                fos = new FileOutputStream(finalPath);
                byte[] buf = new byte[1024];
                int n;
                while ((n = fis.read(buf)) > 0)
                    fos.write(buf, 0, n);
            } catch (IOException e) {
                Free42Activity.showAlert("State duplication failed.");
            } finally {
                if (fis != null)
                    try {
                        fis.close();
                    } catch (IOException e) {}
                if (fos != null)
                    try {
                        fos.close();
                    } catch (IOException e) {}
            }
        }
        updateUI(true);
    }

    private void doRename() {
        String selectedState = getSelectedState();
        getStateName("Rename \"" +  selectedState + "\" to:", selectedState, 1);
    }

    private void doRename2(String newStateName) {
        String selectedStateName = getSelectedState();
        if (selectedStateName == null)
            return;
        String oldpath = stateDirName + "/" + selectedStateName + ".f42";
        String newpath = stateDirName + "/" + newStateName + ".f42";
        if (new File(newpath).exists()) {
            Free42Activity.showAlert("That name is already in use.");
            return;
        }
        new File(oldpath).renameTo(new File(newpath));
        if (selectedStateName.equals(Free42Activity.getSelectedState())) {
            currentLabel.setText("Current: " + newStateName);
            Free42Activity.setSelectedState(newStateName);
        }
        updateUI(true);
    }

    private void doDelete() {
        String selectedStateName = getSelectedState();
        if (selectedStateName == null || selectedStateName.equals(Free42Activity.getSelectedState()))
            return;
        new AlertDialog.Builder(getContext())
                .setTitle("Confirm Delete")
                .setMessage("Are you sure you want to delete the state \"" + selectedStateName + "\"?")
                .setIcon(android.R.drawable.ic_dialog_info)
                .setPositiveButton("OK", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int whichButton) {
                        doDelete2();
                    }
                })
                .setNegativeButton("Cancel", null).show();
    }

    private void doDelete2() {
        String selectedStateName = getSelectedState();
        if (selectedStateName == null || selectedStateName.equals(Free42Activity.getSelectedState()))
            return;
        String path = stateDirName + "/" + selectedStateName + ".f42";
        new File(path).delete();
        updateUI(true);
    }

    private void doImport() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        // TODO: Is there any point to putExtra() when opening a document?
        // intent.putExtra(Intent.EXTRA_TITLE, "Importing, eh?");
        Free42Activity.instance.startActivityForResult(intent, Free42Activity.IMPORT_STATE);
    }

    public static void doImport2(Uri uri) {
        instance.doImport3(uri);
    }

    private void doImport3(Uri uri) {
        String name = Free42Activity.getNameFromUri(uri);
        if (name == null || !name.endsWith(".f42"))
            return;
        name = name.substring(0, name.length() - 4);
        String destPath = getContext().getFilesDir() + "/" + name + ".f42";
        if (new File(destPath).exists())
            destPath = getContext().getFilesDir() + "/" + makeCopyName(name) + ".f42";
        BackgroundIO.load(uri, destPath, "State import failed.", new Runnable() { public void run() { updateUI(true); } });
    }

    private void doExport() {
        String selectedStateName = getSelectedState();
        if (selectedStateName == null)
            return;
        Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        intent.putExtra(Intent.EXTRA_TITLE, selectedStateName + ".f42");
        Free42Activity.instance.startActivityForResult(intent, Free42Activity.EXPORT_STATE);
    }

    public static void doExport2(Uri uri) {
        instance.doExport3(uri);
    }

    private void doExport3(Uri uri) {
        String selectedStateName = getSelectedState();
        // How we save the state depends on whether the selected state is the currently
        // active one. If it is, we'll call core_save_state(), to make sure the duplicate
        // actually matches the most up-to-date state; otherwise, we can simply copy
        // the existing state file.
        String origPath = null;
        boolean deleteOrig;
        if (selectedStateName.equals(Free42Activity.getSelectedState())) {
            String tempName = "_TEMP_STATE_";
            origPath = stateDirName + "/" + tempName;
            Free42Activity.saveStateAs(origPath);
            deleteOrig = true;
        } else {
            origPath = stateDirName + "/" + getSelectedState() + ".f42";
            deleteOrig = false;
        }
        BackgroundIO.save(origPath, uri, deleteOrig, "State export failed.", new Runnable() { public void run() { updateUI(true); }});
    }

    private void doShare() {
        String selectedStateName = getSelectedState();
        if (selectedStateName == null)
            return;
        Intent intent = new Intent(Intent.ACTION_SEND);
        File file = new File(stateDirName + "/" + selectedStateName + ".f42");
        Uri uri = FileProvider.getUriForFile(getContext(), getContext().getPackageName() + ".fileprovider", file);
        intent.setType("*/*");
        intent.putExtra(Intent.EXTRA_STREAM, uri);
        getContext().startActivity(Intent.createChooser(intent, "Share Free42 State Using"));
    }

    private class MoreMenuOnClickListener implements DialogInterface.OnClickListener {
        private int mode;
        public MoreMenuOnClickListener(int mode) {
            this.mode = mode;
        }
        public void onClick(DialogInterface dialog, int which) {
            switch (mode) {
                case 0:
                    switch (which) {
                        case 0: doNew(); break;
                        case 1: doImport(); break;
                        // default: Cancel; do nothing
                    }
                    break;
                case 1:
                    switch (which) {
                        case 0: doNew(); break;
                        case 1: doDuplicate(); break;
                        case 2: doRename(); break;
                        case 3: doImport(); break;
                        case 4: doExport(); break;
                        case 5: doShare(); break;
                        // default: Cancel; do nothing
                    }
                    break;
                case 2:
                    switch (which) {
                        case 0: doNew(); break;
                        case 1: doDuplicate(); break;
                        case 2: doRename(); break;
                        case 3: doDelete(); break;
                        case 4: doImport(); break;
                        case 5: doExport(); break;
                        case 6: doShare(); break;
                        // default: Cancel; do nothing
                    }
                    break;
            }
        }
    }

    private void doMore() {
        String selectedState = getSelectedState();
        int mode = selectedState == null ? 0 : selectedState.equals(Free42Activity.getSelectedState()) ? 1 : 2;
        AlertDialog.Builder builder = new AlertDialog.Builder(Free42Activity.instance);
        builder.setTitle("States Menu");
        List<String> itemsList = new ArrayList<String>();
        itemsList.add("New");
        if (mode != 0) {
            itemsList.add("Duplicate");
            itemsList.add("Rename");
            if (mode != 1)
                itemsList.add("Delete");
        }
        itemsList.add("Import");
        if (mode != 0) {
            itemsList.add("Export");
            itemsList.add("Share");
        }
        itemsList.add("Cancel");
        builder.setItems(itemsList.toArray(new String[itemsList.size()]),
                new MoreMenuOnClickListener(mode));
        builder.create().show();
    }

    private void doDone() {
        dismiss();
    }

    private static ArrayList<String> getLoadedStates(String stateDirName) {
        ArrayList<String> nameList = new ArrayList<String>();
        File[] files = new File(stateDirName).listFiles();
        if (files != null)
            for (File file : files) {
                if (!file.isFile())
                    continue;
                String fn = file.getName();
                if (!fn.endsWith(".f42"))
                    continue;
                nameList.add(fn.substring(0, fn.length() - 4));
            }
        Collections.sort(nameList, String.CASE_INSENSITIVE_ORDER);
        return nameList;
    }

    private EditText stateName;
    private int continuation;

    private void getStateName(String prompt, String initialName, int continuation) {
        this.continuation = continuation;
        Context ctx = getContext();
        AlertDialog.Builder builder = new AlertDialog.Builder(ctx);
        builder.setTitle("State Name");
        builder.setMessage(prompt);
        stateName = new EditText(ctx);
        stateName.setText(initialName);
        stateName.setSelectAllOnFocus(true);
        builder.setView(stateName);
        builder.setPositiveButton("OK", new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                switch (StatesDialog.this.continuation) {
                    case 0:
                        doNew2(stateName.getText().toString());
                        break;
                    case 1:
                        doRename2(stateName.getText().toString());
                        break;
                }
                dialog.dismiss();
            }
        });
        builder.setNegativeButton("Cancel", new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                dialog.cancel();
            }
        });
        builder.show();
    }
}

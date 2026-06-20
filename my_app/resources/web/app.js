// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO: Import Mojo JS bindings once WebUI infrastructure is configured:
//   import {NativeApi, NativeApiRemote}
//       from '/my_app/common/mojom/native_api.mojom-webui.js';

const status = document.getElementById('status');

function log(msg) {
  status.textContent = `[${new Date().toLocaleTimeString()}] ${msg}\n` +
      status.textContent;
}

document.getElementById('btn-read-clipboard').addEventListener('click', () => {
  log('Read Clipboard clicked (Mojo binding not yet wired)');
});

document.getElementById('btn-write-clipboard').addEventListener('click', () => {
  log('Write to Clipboard clicked (Mojo binding not yet wired)');
});

document.getElementById('btn-open-file').addEventListener('click', () => {
  log('Open File clicked (Mojo binding not yet wired)');
});

log('App initialized. Mojo JS integration pending WebUI setup.');

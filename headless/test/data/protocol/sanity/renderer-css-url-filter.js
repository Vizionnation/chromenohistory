// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  let {page, session, dp} = await testRunner.startWithFrameControl(
      'Tests renderer: canvas.', {width: 100, height: 100});

  // Ensures that "filter: url(...)" does not get into an infinite style update
  // loop.
  await dp.Runtime.enable();
  await dp.HeadlessExperimental.enable();

  let RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  let {httpInterceptor, frameNavigationHelper, virtualTimeController} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  // The image from circle.svg will be drawn with the blur from blur.svg.
  httpInterceptor.addResponse(
      `http://www.example.com/`,
      `<!DOCTYPE html>
      <style>
        body { margin: 0; }
        img {
          -webkit-filter: url(blur.svg#blur);
          filter: url(blur.svg#blur);
        }
      </style>
      <img src="circle.svg">`);

  // Just a normal image.
  httpInterceptor.addResponse(
      `http://www.example.com/circle.svg`,
      `<svg width="100" height="100" version="1.1"
          xmlns="http://www.w3.org/2000/svg"
        xmlns:xlink="http://www.w3.org/1999/xlink">
        <circle cx="50" cy="50" r="50" fill="green" />
      </svg>`);

  // A blur filter stored inside an svg file.
  httpInterceptor.addResponse(
      `http://www.example.com/blur.svg#blur`,
      `<svg width="100" height="100" version="1.1"
          xmlns="http://www.w3.org/2000/svg"
          xmlns:xlink="http://www.w3.org/1999/xlink">
        <filter id="blur">
          <feGaussianBlur in="SourceGraphic" stdDeviation="5"/>
        </filter>
      </svg>`);

  await virtualTimeController.grantInitialTime(500, 1000,
    null,
    async () => {
      const frameTimeTicks = virtualTimeController.currentFrameTime();
      const screenshotData =
          (await dp.HeadlessExperimental.beginFrame(
              {frameTimeTicks, screenshot: {format: 'png'}}))
          .result.screenshotData;
      await logScreenShotData(screenshotData);
      testRunner.completeTest();
    }
  );

  function logScreenShotData(pngBase64) {
    const image = new Image();

    let callback;
    let promise = new Promise(fulfill => callback = fulfill);
    image.onload = function() {
      testRunner.log(`Screenshot size: `
          + `${image.naturalWidth} x ${image.naturalHeight}`);
      const canvas = document.createElement('canvas');
      canvas.width = image.naturalWidth;
      canvas.height = image.naturalHeight;
      const ctx = canvas.getContext('2d');
      ctx.drawImage(image, 0, 0);
      for (let n = 0; n < 25; ++n) {
        const rgba = ctx.getImageData(n, n, 1, 1).data;
        testRunner.log(`rgba @(${n},${n}): ${rgba}`);
      }

      callback();
    }

    image.src = `data:image/png;base64,${pngBase64}`;

    return promise;
  }

  await frameNavigationHelper.navigate('http://www.example.com/');
})

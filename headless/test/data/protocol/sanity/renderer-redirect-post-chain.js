// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  let {page, session, dp} = await testRunner.startWithFrameControl(
      'Tests renderer: post chain redirection.');

  let RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  let {httpInterceptor, frameNavigationHelper, virtualTimeController} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  httpInterceptor.addResponse('http://www.example.com/',
      `<html>
        <body onload='document.forms[0].submit();'>
          <form action='1' method='post'>
            <input name='foo' value='bar'>
          </form>
          </body>
      </html>`);

  httpInterceptor.addResponse('http://www.example.com/1', null,
      ['HTTP/1.1 307 Temporary Redirect', 'Location: /2']);

  httpInterceptor.addResponse('http://www.example.com/2',
      `<html>
        <body onload='document.forms[0].submit();'>
          <form action='3' method='post'>
          </form>
          </body>
      </html>`);

  httpInterceptor.addResponse('http://www.example.com/3', null,
      ['HTTP/1.1 307 Temporary Redirect', 'Location: /4']);

  httpInterceptor.addResponse('http://www.example.com/4',
      '<p>Pass</p>');

  await virtualTimeController.grantInitialTime(1000, 1000,
    null,
    async () => {
      testRunner.log(await session.evaluate('document.body.innerHTML'));
      httpInterceptor.logRequestedMethods();
      frameNavigationHelper.logFrames();
      frameNavigationHelper.logScheduledNavigations();
      testRunner.completeTest();
    }
  );

  await frameNavigationHelper.navigate('http://www.example.com/');
})

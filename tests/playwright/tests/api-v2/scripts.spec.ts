import { test, expect } from '@playwright/test';

const V2 = '/api/v2';

test.describe('scripts', () => {

  test('GET /scripts', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/scripts`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(Array.isArray(body)).toBe(true);
  });

  test('POST /scripts/installRemote/:category/:filename', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: internet access and valid remote script category/filename
    test.skip(true, 'Requires internet access and a valid remote script category and filename');
  });

  test('GET /scripts/viewRemote/:category/:filename', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: internet access and valid remote script category/filename
    test.skip(true, 'Requires internet access and a valid remote script category and filename');
  });

  test('GET /scripts/:scriptName', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: a script to exist on the device with a known name
    test.skip(true, 'Requires a pre-existing script on the device');
  });

  test('POST /scripts/:scriptName — save and clean up', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    const scriptName = 'ci-test-script.sh';
    const saveRes = await request.post(`${V2}/scripts/${scriptName}`, {
      data: { content: '#!/bin/bash\necho "ci test"\n' },
    });
    expect(saveRes.status()).toBe(200);

    // Clean up by deleting via file endpoint since scripts are stored as files
    await request.delete(`${V2}/file/scripts/${scriptName}`);
  });

  test('POST /scripts/:scriptName/run', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: a script to exist on the device with a known name
    test.skip(true, 'Requires a pre-existing script on the device');
  });

});

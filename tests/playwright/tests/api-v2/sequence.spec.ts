import { test, expect } from '@playwright/test';

const V2 = '/api/v2';

test.describe('sequence', () => {

  test('GET /sequence', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/sequence`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(Array.isArray(body)).toBe(true);
  });

  test('POST /sequence/current/step', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: a sequence currently paused in fppd
    test.skip(true, 'Requires a paused sequence running in fppd');
  });

  test('POST /sequence/current/stop', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: a sequence currently running in fppd
    test.skip(true, 'Requires a running sequence in fppd');
  });

  test('POST /sequence/current/togglePause', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: a sequence currently running in fppd
    test.skip(true, 'Requires a running sequence in fppd');
  });

  test('GET /sequence/:SequenceName', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: a .fseq file to exist with a known name
    test.skip(true, 'Requires a pre-existing .fseq sequence file on the device');
  });

  test('GET /sequence/:SequenceName/meta', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: a .fseq file to exist with a known name
    test.skip(true, 'Requires a pre-existing .fseq sequence file on the device');
  });

  test('POST /sequence/:SequenceName/start/:startSecond', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: a .fseq file to exist; starting also requires fppd to be in a playable state
    test.skip(true, 'Requires a pre-existing .fseq sequence file and fppd in playable state');
  });

  test('POST /sequence/:SequenceName then DELETE — upload then remove', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Uploading a valid .fseq requires actual binary FSEQ content; skip rather than send invalid data
    test.skip(true, 'Requires a valid binary .fseq file to upload');
  });

});

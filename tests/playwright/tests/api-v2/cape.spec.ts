import { test, expect } from '@playwright/test';

const V2 = '/api/v2';

test.describe('cape', () => {

  test('GET /cape', { tag: ['@hardware:cape', '@schema'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/cape`);
    expect(res.status()).toBe(404);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /cape/eeprom/voucher', { tag: ['@notest'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'NOTEST' });
    // Requires: valid key and order values from a real cape provisioning workflow
    test.skip(true, 'Requires valid cape signing key and order');
  });

  test('POST /cape/eeprom/sign/:key/:order', { tag: ['@notest'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'NOTEST' });
    // Requires: valid key and order values from a real cape provisioning workflow
    test.skip(true, 'Requires valid cape signing key and order');
  });

  test('GET /cape/eeprom/signingData/:key/:order', { tag: ['@notest'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'NOTEST' });
    // Requires: valid key and order values from a real cape provisioning workflow
    test.skip(true, 'Requires valid cape signing key and order');
  });

  test('GET /cape/eeprom/signingFile/:key/:order', { tag: ['@notest'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'NOTEST' });
    // Requires: valid key and order values from a real cape provisioning workflow
    test.skip(true, 'No test written. Requires valid cape signing key and order');
  });

  test('POST /cape/eeprom/signingData', { tag: ['@notest'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'NOTEST' });
    // Requires: valid key and order values from a real cape provisioning workflow
    test.skip(true, 'No test written. Requires valid cape signing key and order');
  });

  test('GET /cape/options', { tag: ['@schema'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/cape/options`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(body[0]).toBe("--None--");
    expect(Array.isArray(body)).toBe(true);
  });

  test('GET /cape/strings', { tag: ['@schema'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/cape/strings`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(Array.isArray(body)).toBe(true);
  });

  test('GET /cape/panel', { tag: ['@schema'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/cape/panel`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(Array.isArray(body)).toBe(true);
  });

  test('GET /cape/strings/:key', { tag: ['@notest'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'NOTEST' });
    // Requires: cape with string outputs configured; key must be a valid string config key
    test.skip(true, 'No test written. Requires physical cape with string outputs');
  });

  test('GET /cape/panel/:key', { tag: ['@notest'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'NOTEST' });
    // Requires: cape with panel outputs configured; key must be a valid panel config key
    test.skip(true, 'No test written. Requires physical cape with panel outputs');
  });

});

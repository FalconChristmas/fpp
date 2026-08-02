import { test, expect } from '@playwright/test';

const V2 = '/api/v2';

test.describe('settings', () => {

  test('GET /settings', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/settings`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('GET /settings/:SettingName', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/settings/fppMode`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(typeof body).toBe('string');
  });

  test('GET /settings/:SettingName/options', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/settings/fppMode/options`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(Array.isArray(body)).toBe(true);
  });

  test('PUT /settings/:SettingName — change and restore', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Read current value of a safe, non-disruptive setting
    const getRes = await request.get(`${V2}/settings/uiLevel`);
    expect(getRes.status()).toBe(200);
    const original = await getRes.json();

    const putRes = await request.put(`${V2}/settings/uiLevel`, { data: original });
    expect(putRes.status()).toBe(200);

    // Restore original value
    await request.put(`${V2}/settings/uiLevel`, { data: original });
  });

  test('PUT /settings/:SettingName/jsonValueUpdate', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: a JSON-type setting name and a valid update payload
    test.skip(true, 'Requires knowledge of a JSON-type setting and valid update structure');
  });

});

import { test, expect } from '@playwright/test';

const V2 = '/api/v2';

test.describe('schedule', () => {

  test('GET /schedule', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/schedule`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
    expect(body).toHaveProperty('scheduleEntries');
    expect(Array.isArray(body.scheduleEntries)).toBe(true);
  });

  test('POST /schedule — save current schedule back', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Read existing schedule and write it back unchanged to avoid data loss
    const getRes = await request.get(`${V2}/schedule`);
    expect(getRes.status()).toBe(200);
    const original = await getRes.json();

    const saveRes = await request.post(`${V2}/schedule`, { data: original });
    expect(saveRes.status()).toBe(200);
  });

  test('POST /schedule/reload', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    const res = await request.post(`${V2}/schedule/reload`);
    expect(res.status()).toBe(200);
  });

});

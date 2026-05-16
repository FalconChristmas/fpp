import { test, expect } from '@playwright/test';

const V2 = '/api/v2';

test.describe('options', () => {

  test('GET /options/:SettingName', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/options/fppMode`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(Array.isArray(body)).toBe(true);
    expect(body.length).toBeGreaterThan(0);
    for (const item of body) {
      expect(typeof item).toBe('object');
    }
  });

});

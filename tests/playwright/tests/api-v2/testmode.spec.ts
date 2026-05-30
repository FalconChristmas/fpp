import { test, expect } from '@playwright/test';

const V2 = '/api/v2';

test.describe('testmode', () => {

  test('GET /testmode', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'FULL' });
    const res = await request.get(`${V2}/testmode`);
    expect(res.status()).toBe(200);
    expect(await res.json()).toEqual({ enabled: false });
  });

  test('POST /testmode — enable then disable', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    const enable = await request.post(`${V2}/testmode`, {
      data: { enabled: true },
    });
    expect(enable.status()).toBe(200);

    const disable = await request.post(`${V2}/testmode`, {
      data: { enabled: false },
    });
    expect(disable.status()).toBe(200);
  });

});

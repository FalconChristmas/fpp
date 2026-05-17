import { test, expect } from '@playwright/test';

const V2 = '/api/v2';

test.describe('time', () => {

  test('GET /time', { tag: ['@full'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'FULL' });
    const res = await request.get(`${V2}/time`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).toHaveProperty('utcDate');
    expect(body).toHaveProperty('localDate');
    expect(body).toHaveProperty('utcOffset');
    expect(typeof body.utcDate).toBe('string');
    expect(typeof body.localDate).toBe('string');
  });

});

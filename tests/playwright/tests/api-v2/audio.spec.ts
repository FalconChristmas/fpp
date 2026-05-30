import { test, expect } from '@playwright/test';

const V2 = '/api/v2';

test.describe('audio', () => {

  test('GET /audio/cardaliases', { tag: ['@hardware:audio', '@schema'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/audio/cardaliases`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /audio/cardaliases', { tag: ['@hardware:audio', '@schema'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.post(`${V2}/audio/cardaliases`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

});

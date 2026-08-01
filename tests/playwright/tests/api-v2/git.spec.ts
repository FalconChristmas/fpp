import { test, expect } from '@playwright/test';

const V2 = '/api/v2';

test.describe('git', () => {

  test('GET /git/originLog', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/git/originLog`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(Array.isArray(body)).toBe(true);
  });

  test('GET /git/releases/os/:All', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/git/releases/os/0`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(Array.isArray(body)).toBe(true);
  });

  test('GET /git/releases/sizes', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/git/releases/sizes`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('GET /git/status', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/git/status`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('GET /git/branches', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/git/branches`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(Array.isArray(body)).toBe(true);
  });

  test('POST /git/reset', { tag: ['@destructive'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'DESTRUCTIVE' });
    // Resets git repository state — discards local modifications
    const res = await request.post(`${V2}/git/reset`);
    expect([200, 202]).toContain(res.status());
  });

});

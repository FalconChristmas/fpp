import { test, expect } from '@playwright/test';

const V2 = '/api/v2';

test.describe('files', () => {

  test('GET /files/:DirName', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/files/sequences`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(Array.isArray(body)).toBe(true);
  });

  test('GET /files/zip/:DirNames', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/files/zip/sequences`);
    expect(res.status()).toBe(200);
    const contentType = res.headers()['content-type'] || '';
    expect(contentType).toMatch(/zip|octet-stream/i);
  });

});

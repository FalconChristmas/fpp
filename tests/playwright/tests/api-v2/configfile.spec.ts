import { test, expect } from '@playwright/test';

const V2 = '/api/v2';

test.describe('configfile', () => {

  test('GET /configfile', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/configfile`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(Array.isArray(body)).toBe(true);
  });

  test('GET /configfile/** — download known config', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/configfile/settings`);
    expect(res.status()).toBe(200);
  });

  test('POST /configfile/** then DELETE — upload then remove', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    const content = '# CI test config file\n';
    const uploadRes = await request.post(`${V2}/configfile/ci-test-config.txt`, {
      headers: { 'Content-Type': 'text/plain' },
      data: content,
    });
    expect(uploadRes.status()).toBe(200);

    const delRes = await request.delete(`${V2}/configfile/ci-test-config.txt`);
    expect(delRes.status()).toBe(200);
  });

});

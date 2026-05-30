import { test, expect } from '@playwright/test';

const V2 = '/api/v2';

test.describe('configfile', () => {
  const traversalReadPath = '../fpp-info.json';
  const traversalWritePath = '../evil.txt';

  test('End-to-End /configfile Test', { tag: ['@e2e'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'END2END' });
    const testPath = 'ci/e2e-configfile.txt';
    const content = '# CI test config file\n';

    const listRes = await request.get(`${V2}/configfile`); // GET /configfile
    expect(listRes.status()).toBe(200);
    const listBody = await listRes.json();
    expect(listBody).toHaveProperty('Path', '');
    expect(Array.isArray(listBody.ConfigFiles)).toBe(true);

    const uploadRes = await request.post(`${V2}/configfile/${testPath}`, { // POST /configfile/**
      headers: { 'Content-Type': 'text/plain' },
      data: content,
    });
    expect(uploadRes.status()).toBe(200);
    expect(await uploadRes.json()).toEqual({ Status: 'OK', Message: '' });

    const downloadRes = await request.get(`${V2}/configfile/${testPath}`); // GET /configfile/**
    expect(downloadRes.status()).toBe(200);
    expect(await downloadRes.text()).toBe(content);

    const delRes = await request.delete(`${V2}/configfile/${testPath}`); // DELETE /configfile/**
    expect(delRes.status()).toBe(200);
    expect(await delRes.json()).toEqual({ Status: 'OK', Message: '' });
  });

  test('GET /configfile', { tag: ['@covered'] }, async () => {
    test.info().annotations.push({ type: 'tier', description: 'COVERED' });
    test.skip(true, 'Covered inline by End-to-End /configfile test');
  });

  test('POST /configfile/**', { tag: ['@covered'] }, async () => {
    test.info().annotations.push({ type: 'tier', description: 'COVERED' });
    test.skip(true, 'Covered inline by End-to-End /configfile test');
  });

  test('GET /configfile/**', { tag: ['@covered'] }, async () => {
    test.info().annotations.push({ type: 'tier', description: 'COVERED' });
    test.skip(true, 'Covered inline by End-to-End /configfile test');
  });

  test('DELETE /configfile/**', { tag: ['@covered'] }, async () => {
    test.info().annotations.push({ type: 'tier', description: 'COVERED' });
    test.skip(true, 'Covered inline by End-to-End /configfile test');
  });

  test('GET /configfile/** blocks ../fpp-info.json traversal', { tag: ['@full', '@security'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SECURITY' });
    const res = await request.get(`${V2}/configfile/${traversalReadPath}`);
    expect(res.status()).toBe(404);
  });

  test('POST /configfile/** blocks ../evil.txt traversal and follow-up read', { tag: ['@full', '@security'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SECURITY' });
    const postRes = await request.post(`${V2}/configfile/${traversalWritePath}`, {
      headers: { 'Content-Type': 'text/plain' },
      data: 'hello world',
    });
    expect(postRes.status()).toBe(404);

    const getRes = await request.get(`${V2}/configfile/${traversalWritePath}`);
    expect(getRes.status()).toBe(404);
  });

  test('DELETE /configfile/** blocks ../fpp-info.json traversal', { tag: ['@full', '@security'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SECURITY' });
    const res = await request.delete(`${V2}/configfile/${traversalReadPath}`);
    expect(res.status()).toBe(404);
  });

});

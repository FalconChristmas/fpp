import { test, expect } from '@playwright/test';

const V2 = '/api/v2';

test.describe('dir', () => {

  test('POST /dir/:DirName/:SubDir then DELETE — create then remove', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    const createRes = await request.post(`${V2}/dir/sequences/ci-test-dir`);
    expect(createRes.status()).toBe(200);

    const delRes = await request.delete(`${V2}/dir/sequences/ci-test-dir`);
    expect(delRes.status()).toBe(200);
  });

});

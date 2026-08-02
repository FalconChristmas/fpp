import { test, expect } from '@playwright/test';

const V2 = '/api/v2';

test.describe('dir', () => {

  test('End-to-End /dir Test', { tag: ['@e2e'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'END2END' });
    const dirName = 'sequences';
    const subdir = 'ci-test-dir';

    // Delete if exists
    const cleanupRes = await request.delete(`${V2}/dir/${dirName}/${subdir}`); // DELETE /dir/{DirName}/{SubDir}
    expect(cleanupRes.status()).toBe(200);

    // Create
    const createRes = await request.post(`${V2}/dir/${dirName}/${subdir}`); // POST /dir/{DirName}/{SubDir}
    expect(createRes.status()).toBe(200);
    expect(await createRes.json()).toEqual({
      status: 'OK',
      subdir,
      dir: dirName,
    });

    // Attempt to create again, get failure
    const createAgainRes = await request.post(`${V2}/dir/${dirName}/${subdir}`);
    expect(createAgainRes.status()).toBe(200);
    expect(await createAgainRes.json()).toEqual({
      status: 'Subdirectory already exists',
      subdir,
      dir: dirName,
    });

    // Delete
    const delRes = await request.delete(`${V2}/dir/${dirName}/${subdir}`);
    expect(delRes.status()).toBe(200);
    expect(await delRes.json()).toEqual({
      status: 'OK',
      subdir,
      dir: dirName,
    });
  });

  test('POST /dir/**', async () => {
    test.info().annotations.push({ type: 'tier', description: 'COVERED' });
    test.skip(true, 'Covered inline by End-to-End /dir test');
  });

  test('DELETE /dir/**', async () => {
    test.info().annotations.push({ type: 'tier', description: 'COVERED' });
    test.skip(true, 'Covered inline by End-to-End /dir test');
  });

});

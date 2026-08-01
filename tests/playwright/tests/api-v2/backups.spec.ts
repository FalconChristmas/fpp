import { test, expect } from '@playwright/test';

const V2 = '/api/v2';

test.describe('backups', () => {

  test('GET /backups/list', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/backups/list`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(Array.isArray(body)).toBe(true);
  });

  test('GET /backups/list/:DeviceName', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: external device mounted with a known DeviceName
    test.skip(true, 'Requires mounted external backup device');
  });

  test('GET /backups/devices', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/backups/devices`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(Array.isArray(body)).toBe(true);
  });

  test('POST /backups/devices/mount/:DeviceName/:MountLocation', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: a known removable device path on the system
    test.skip(true, 'Requires a physically attached backup device');
  });

  test('POST /backups/devices/unmount/:DeviceName/:MountLocation', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: a mounted device that can be safely unmounted
    test.skip(true, 'Requires a mounted backup device');
  });

  test('POST /backups/configuration — create then delete', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    const create = await request.post(`${V2}/backups/configuration`);
    expect(create.status()).toBe(200);
    const body = await create.json();
    expect(body).toHaveProperty('filename');
    expect(typeof body.filename).toBe('string');

    const filename: string = body.filename;
    // Backups are stored under a directory — split on the last '/' to get dir and file
    const lastSlash = filename.lastIndexOf('/');
    const dir = lastSlash >= 0 ? filename.substring(0, lastSlash) : '.';
    const file = lastSlash >= 0 ? filename.substring(lastSlash + 1) : filename;

    const del = await request.delete(`${V2}/backups/configuration/${dir}/${file}`);
    expect(del.status()).toBe(200);
  });

  test('GET /backups/configuration/list', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/backups/configuration/list`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(Array.isArray(body)).toBe(true);
  });

  test('GET /backups/configuration/list/:DeviceName', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: external device mounted at a known DeviceName
    test.skip(true, 'Requires mounted external backup device');
  });

  test('POST /backups/configuration/restore/:Directory/:BackupFilename', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: a backup to exist at a known directory/filename
    test.skip(true, 'Requires a pre-existing backup file');
  });

  test('GET /backups/configuration/:Directory/:BackupFilename', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: a backup to exist at a known directory/filename
    test.skip(true, 'Requires a pre-existing backup file');
  });

  test('DELETE /backups/configuration/:Directory/:BackupFilename', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Covered inline by the POST /backups/configuration STATE test above
    test.skip(true, 'Covered inline by POST /backups/configuration test');
  });

});

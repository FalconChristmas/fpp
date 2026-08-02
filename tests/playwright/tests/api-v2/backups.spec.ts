import { test, expect } from '@playwright/test';

const V2 = '/api/v2';

test.describe('backups', () => {

  test('End-to-End Backup and Restore', { tag: ['@e2e'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'END2END' });
    const backupList = await request.get(`${V2}/backups/list`); // GET /backups/list
    expect(backupList.status()).toBe(200);
    const backupListBody = await backupList.json();
    expect(Array.isArray(backupListBody)).toBe(true);

    const create = await request.post(`${V2}/backups/configuration`);  // POST /backups/configuration
    expect(create.status()).toBe(200);
    const body = await create.json();
    expect(body).toHaveProperty('success', true);
    expect(body).toHaveProperty('backup_file_path');
    expect(typeof body.backup_file_path).toBe('string');

    const createdBackupPath: string = body.backup_file_path;
    const file = createdBackupPath.substring(createdBackupPath.lastIndexOf('/') + 1);

    const list = await request.get(`${V2}/backups/configuration/list`); // GET /backups/configuration/list
    expect(list.status()).toBe(200);
    const backups = await list.json();
    expect(Array.isArray(backups)).toBe(true);
    expect(backups.length).toBeGreaterThan(0);
    expect(backups[0]).toHaveProperty('backup_filedirectory');
    expect(backups[0]).toHaveProperty('backup_filename');

    const listedBackupPath = `${String(backups[0].backup_filedirectory).replace(/\/$/, '')}/${backups[0].backup_filename}`;
    expect(listedBackupPath).toBe(createdBackupPath);

    const directory = backups[0].backup_alternative_location ? 'JsonBackupsAlternate' : 'JsonBackups';

    const download = await request.get(`${V2}/backups/configuration/${directory}/${file}`); // GET /backups/configuration/:Directory/:BackupFilename
    expect(download.status()).toBe(200);
    expect(download.headers()['content-type']).toContain('application/json');
    const downloadedBackup = JSON.parse(await download.text());
    expect(downloadedBackup).toBeTruthy();
    expect(typeof downloadedBackup).toBe('object');

    const restore = await request.post(`${V2}/backups/configuration/restore/${directory}/${file}`, { // POST /backups/configuration/restore/:Directory/:BackupFilename
      headers: { 'Content-Type': 'text/plain' },
      data: 'all',
    });
    expect(restore.status()).toBe(200);
    const restoreBody = await restore.json();
    expect(restoreBody.Success).toBeTruthy();

    const del = await request.delete(`${V2}/backups/configuration/JsonBackups/${file}`); // DELETE /backups/configuration/:Directory/:BackupFilename
    expect(del.status()).toBe(200);
  });

  test('GET /backups/list', { tag: ['@schema'] }, async () => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    // Covered inline by the End to End Backup and Restore STATE test above
    test.skip(true, 'Covered inline by End to End Backup and Restore test');
  });

  test('GET /backups/list/:DeviceName', { tag: ['@notest'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'NOTEST' });
    // Requires: external device mounted with a known DeviceName
    test.skip(true, 'No test written. Requires mounted external backup device');
  });

  test('GET /backups/devices', { tag: ['@schema'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/backups/devices`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(Array.isArray(body)).toBe(true);
  });

  test('GET /backups/configuration/list/:DeviceName', { tag: ['@hardware:external'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    // Requires: external device mounted at a known DeviceName
    test.skip(true, 'Requires mounted external backup device');
  });

  test('POST /backups/devices/mount/:DeviceName/:MountLocation', { tag: ['@notest'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'NOTEST' });
    // Requires: a known removable device path on the system
    test.skip(true, 'No test written. Requires a physically attached backup device');
  });

  test('POST /backups/devices/unmount/:DeviceName/:MountLocation', { tag: ['@notest'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'NOTEST' });
    // Requires: a mounted device that can be safely unmounted
    test.skip(true, 'No test written. Requires a mounted backup device');
  });

  test('POST /backups/configuration', { tag: ['@covered'] }, async () => {
    test.info().annotations.push({ type: 'tier', description: 'COVERED' });
    test.skip(true, 'Covered inline by End to End Backup and Restore test');
  });

  test('GET /backups/configuration/list', { tag: ['@covered'] }, async () => {
    test.info().annotations.push({ type: 'tier', description: 'COVERED' });
    test.skip(true, 'Covered inline by End to End Backup and Restore test');
  });

  test('POST /backups/configuration/restore/:Directory/:BackupFilename', { tag: ['@covered'] }, async () => {
    test.info().annotations.push({ type: 'tier', description: 'COVERED' });
    test.skip(true, 'Covered inline by End to End Backup and Restore test');
  });

  test('GET /backups/configuration/:Directory/:BackupFilename', { tag: ['@covered'] }, async () => {
    test.info().annotations.push({ type: 'tier', description: 'COVERED' });
    test.skip(true, 'Covered inline by End to End Backup and Restore test');
  });

  test('DELETE /backups/configuration/:Directory/:BackupFilename', { tag: ['@covered'] }, async () => {
    test.info().annotations.push({ type: 'tier', description: 'COVERED' });
    test.skip(true, 'Covered inline by End to End Backup and Restore test');
  });

});

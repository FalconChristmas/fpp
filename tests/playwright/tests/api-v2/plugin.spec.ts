import { test, expect } from '@playwright/test';

const V2 = '/api/v2';

test.describe('plugin', () => {

  test('GET /plugin/headerIndicators', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/plugin/headerIndicators`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(Array.isArray(body)).toBe(true);
  });

  test('GET /plugin', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/plugin`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(Array.isArray(body)).toBe(true);
  });

  test('POST /plugin — install from URL', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: internet access and a valid plugin repository URL
    test.skip(true, 'Requires internet access and a valid plugin repository URL');
  });

  test('POST /plugin/fetchInfo', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: internet access and a valid plugin repository URL
    test.skip(true, 'Requires internet access and a valid plugin repository URL');
  });

  test('GET /plugin/:RepoName', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: a plugin to be installed on the device
    test.skip(true, 'Requires an installed plugin');
  });

  test('DELETE /plugin/:RepoName', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: a plugin to be installed on the device; irreversible without reinstall
    test.skip(true, 'Requires an installed plugin; uninstall is irreversible without reinstall');
  });

  test('GET /plugin/:RepoName/settings/:SettingName', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: an installed plugin with a known setting name
    test.skip(true, 'Requires an installed plugin with a known setting name');
  });

  test('PUT /plugin/:RepoName/settings/:SettingName', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: an installed plugin with a known writable setting name
    test.skip(true, 'Requires an installed plugin with a known writable setting name');
  });

  test('POST /plugin/:RepoName/settings/:SettingName', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: an installed plugin with a known writable setting name
    test.skip(true, 'Requires an installed plugin with a known writable setting name');
  });

  test('POST /plugin/:RepoName/updates', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: an installed plugin and internet access
    test.skip(true, 'Requires an installed plugin and internet access');
  });

  test('POST /plugin/:RepoName/upgrade', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: an installed plugin with an available upgrade
    test.skip(true, 'Requires an installed plugin with an available upgrade');
  });

});

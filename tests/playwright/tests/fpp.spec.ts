import { test, expect } from './fixtures';
import type { Page } from '@playwright/test';

const SETTINGS_TABS = [
  { slug: 'playback', title: 'Playback' },
  { slug: 'av', title: 'Audio/Video' },
  { slug: 'ui', title: 'UI' },
  { slug: 'privacy', title: 'Privacy' },
  { slug: 'logs', title: 'Logging' },
  { slug: 'services', title: 'Services' },
  { slug: 'system', title: 'System' }
];

async function gotoPage(page: Page, path: string) {
  let lastError: unknown;

  for (let attempt = 1; attempt <= 4; attempt++) {
    try {
      await page.goto(path, { waitUntil: 'commit', timeout: 5_000 });
      return;
    } catch (error) {
      lastError = error;
      if (attempt === 4) {
        break;
      }
      await page.waitForTimeout(750);
    }
  }

  throw lastError;
}

test('Index Overview', async ({ page }) => {
  await gotoPage(page, '/index.php');
  await page.locator('#bodyWrapper').first().waitFor({ state: 'visible', timeout: 10_000 });
  await page.waitForTimeout(1200);
  await page.evaluate(() => window.scrollTo(0, 0));
});

test('Initial Setup Overview', async ({ page }) => {
  await gotoPage(page, '/initialSetup.php');
  await page.locator('#initialSetup').first().waitFor({ state: 'visible', timeout: 10_000 });
  await page.waitForTimeout(250);
});

test('Backup File Copy Error State', async ({ page }) => {
  await gotoPage(page, '/backup.php');
  await page.locator('#fppBackupTabs').first().waitFor({ state: 'visible', timeout: 10_000 });
  await page.click('#backups-fileCopy-tab');
  await page.waitForTimeout(300);
  await page.click('input[value="Copy"]');
  await page.waitForTimeout(500);
});

test('Backup JSON Backup Tab', async ({ page }) => {
  await gotoPage(page, '/backup.php');
  await page.locator('#fppBackupTabs').first().waitFor({ state: 'visible', timeout: 10_000 });

  const jsonBackupTab = page.locator('#backups-jsonBackup-tab').first();
  if (await jsonBackupTab.count()) {
    await jsonBackupTab.click();
  }

  await page.waitForTimeout(300);
});

test('File Manager Logs', async ({ page }) => {
  await gotoPage(page, '/filemanager.php');
  await page.locator('#fileManagerTabs').first().waitFor({ state: 'visible', timeout: 10_000 });

  const logsTab = page.locator('#tab-logs-tab').first();
  if (await logsTab.count()) {
    await logsTab.click();
  }

  await page.waitForTimeout(500);
});

test('Network Overview', async ({ page }) => {
  await gotoPage(page, '/networkconfig.php');
  await page.locator('h1.title').first().waitFor({ state: 'visible', timeout: 10_000 });

  const interfaceSettingsTab = page.locator('#interface-settings-tab').first();
  if (await interfaceSettingsTab.count()) {
    await interfaceSettingsTab.click();
  }

  await page.waitForTimeout(400);
});

test('Playlists Add New Playlist', async ({ page }) => {
  const name = 'PlaywrightNewPlaylist';
  await page.request.delete(`/api/playlist/${name}`);

  // Adding a playlist once loaded 'api/playlist/null' before the new one,
  // which became an error dialog when a missing playlist started to 404.
  const playlistGets: string[] = [];
  page.on('request', (req) => {
    const m = req.url().match(/\/api\/playlist\/([^?]*)/);
    if (m && req.method() === 'GET') {
      playlistGets.push(decodeURIComponent(m[1]));
    }
  });

  try {
    await gotoPage(page, '/playlists.php');
    await page.locator('.playlistAddNewBtn').first().click();
    await page.fill('#txtAddPlaylistName', name);
    await page.locator('#AddPlaylistDialog button', { hasText: 'Add' }).first().click();

    await expect(page.locator('#playlistEditor')).toHaveClass(/hasPlaylistDetailsLoaded/);
    await expect(page.locator('.playlistEditorHeaderTitle').first()).toHaveText(name);
    // The stray request went out before the real one, so it is recorded by now.
    await expect.poll(() => playlistGets.includes(name)).toBe(true);
    expect(playlistGets).toEqual([name]);
    await expect(page.locator('.modal.show')).toHaveCount(0);
    expect(await page.evaluate(() => history.state)).toEqual({ view: 'editor', playlist: name });
  } finally {
    await page.request.delete(`/api/playlist/${name}`);
  }
});

test('Playlists Unsaved Changes Prompt', async ({ page }) => {
  const name = 'PlaywrightUnsavedPlaylist';
  await page.request.delete(`/api/playlist/${name}`);
  await page.request.post(`/api/playlist/${name}`, {
    data: { name, version: 4, leadIn: [], mainPlaylist: [], leadOut: [], playlistInfo: {} }
  });
  const onDisk = async () => (await (await page.request.get(`/api/playlist/${name}`)).json()).mainPlaylist.length;
  const dialog = page.locator('#UnsavedPlaylistDialog.show');

  try {
    await gotoPage(page, '/playlists.php');
    await page.locator('.playlistCard h3', { hasText: new RegExp(`^${name}$`) }).click();
    await expect(page.locator('#playlistEditor')).toHaveClass(/hasPlaylistDetailsLoaded/);

    await page.locator('.playlistEntriesAddNewBtn').first().click();
    await page.selectOption('#pe_type', 'pause');
    await page.locator('.ui-dialog:visible button, .modal.show button', { hasText: /^Add$/ }).first().click();
    await expect(page.locator('tr.playlistRow')).toHaveCount(1);

    // Leaving through the FPP menu used to drop the edit without a word.
    await page.evaluate(() => (document.querySelector('a[href="about.php"]') as HTMLElement).click());
    await expect(dialog).toBeVisible();
    await dialog.locator('button', { hasText: 'Cancel' }).click();
    await expect(dialog).toHaveCount(0);
    await expect(page).toHaveURL(/playlists\.php/);
    await expect(page.locator('tr.playlistRow')).toHaveCount(1);

    await page.evaluate(() => (document.querySelector('a[href="about.php"]') as HTMLElement).click());
    await dialog.locator('button', { hasText: 'Save' }).click();
    await expect(page).toHaveURL(/about\.php/);
    expect(await onDisk()).toBe(1);
  } finally {
    await page.request.delete(`/api/playlist/${name}`);
  }
});

test('Plugins Overview', async ({ page }) => {
  await gotoPage(page, '/plugins.php');
  await page.locator('#bodyWrapper').first().waitFor({ state: 'visible', timeout: 10_000 });
  await page.waitForTimeout(1500);

  const checkAllUpdatesButton = page.locator('#checkAllUpdatesBtn').first();
  if (await checkAllUpdatesButton.count()) {
    await checkAllUpdatesButton.click();
  }

  await page.waitForTimeout(900);
});

test('Scheduler Overview', async ({ page }) => {
  await gotoPage(page, '/scheduler.php');
  await page.locator('h1.title').first().waitFor({ state: 'visible', timeout: 10_000 });

  const addScheduleButton = page.locator('button[onclick="AddScheduleEntry();"]').first();
  if (await addScheduleButton.count()) {
    await addScheduleButton.click();
  }

  await page.waitForTimeout(400);
});

test('System Stats Overview', async ({ page }) => {
  await gotoPage(page, '/system-stats.php');
  await page.locator('h1.title').first().waitFor({ state: 'visible', timeout: 10_000 });
  await page.waitForTimeout(400);
});

test('System Upgrade Overview', async ({ page }) => {
  await gotoPage(page, '/system-upgrade.php');
  await page.locator('h1.title').first().waitFor({ state: 'visible', timeout: 10_000 });
  await page.waitForTimeout(400);
});

for (const tab of SETTINGS_TABS) {
  test(`Settings Tab - ${tab.title}`, async ({ page }) => {
    await gotoPage(page, '/settings.php');
    await page.locator('#settingsManagerTabs').first().waitFor({ state: 'visible', timeout: 10_000 });

    const tabLocator = page.locator(`#settings-${tab.slug}-tab`).first();
    await tabLocator.click();

    await page.waitForFunction(
      ([tabSelector, paneSelector]) => {
        const selectedTab = document.querySelector(tabSelector);
        const selectedPane = document.querySelector(paneSelector);

        return Boolean(
          selectedTab &&
          selectedPane &&
          selectedTab.classList.contains('active') &&
          selectedPane.classList.contains('active')
        );
      },
      [`#settings-${tab.slug}-tab`, `#settings-${tab.slug}`]
    );

    await tabLocator.scrollIntoViewIfNeeded();
    await page.waitForTimeout(250);
  });
}

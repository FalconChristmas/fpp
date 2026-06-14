import { test, expect } from './fixtures';
import type { Page } from '@playwright/test';

// ============================================================
// Mock data — planet-themed synthetic FPP network
//
// Six devices chosen to exercise distinct rendering paths:
//   JUPITER  — local Pi 4, channelInputsEnabled, master branch (up-to-date)
//   SATURN   — SanCloud BeagleBone Enhanced, master (up-to-date), warning row
//   MARS     — remote Pi 4, feature-rgb-output branch (behind), purple bg, syncing
//   VENUS    — Pi 3 B, player+multisync, v9.5-stable (behind), green bg, HostDescription
//   MERCURY  — dual-NIC Pi 4, develop branch (up-to-date), wifi icon; appears twice in
//              MOCK_SYSTEMS (one per IP) with the same uuid so parseFPPSystems() deduplicates
//              them into a single row showing both IPs
//   PLUTO    — typeId 0 (Unknown); never polled; shows only "Last Seen" forever
// ============================================================

const MOCK_SYSTEMS = {
    systems: [
        // JUPITER — local Pi 4, channelInputsEnabled, master branch, up-to-date
        {
            address: '192.168.0.13', hostname: 'JUPITER', local: 1,
            fppMode: 2, fppModeString: 'player',
            typeId: 13, type: 'Raspberry Pi 4', model: 'Raspberry Pi 4 Model B Rev 1.1',
            version: '10.x-master-568-gaaa1111bb', majorVersion: 10, minorVersion: 1000,
            uuid: 'M1-10000000eaa2d726', channelRanges: '0-7',
            channelInputsEnabled: true, channelOutputsEnabled: false,
            multiSyncCapable: 1, multisync: false,
            lastSeen: 1780749445, lastSeenStr: '2026-06-06 07:37:25', HostDescription: '',
        },
        // SATURN — SanCloud BeagleBone Enhanced (typeId 70 = BeagleBone range),
        //          master branch, up-to-date, both channel directions enabled, warning row
        {
            address: '192.168.0.14', hostname: 'SATURN', local: 0,
            fppMode: 2, fppModeString: 'player',
            typeId: 70, type: 'SanCloud BeagleBone Enhanced', model: 'SanCloud BeagleBone Enhanced',
            version: '10.x-master-568-gaaa1111bb', majorVersion: 10, minorVersion: 1000,
            uuid: 'M1-BBE0000000001234', channelRanges: '0-5101',
            channelInputsEnabled: true, channelOutputsEnabled: true,
            multiSyncCapable: 1, multisync: false,
            lastSeen: 1780749438, lastSeenStr: '2026-06-06 07:37:18', HostDescription: '',
        },
        // MARS — remote mode (fppMode=8), feature branch, behind (update available),
        //        purple background, syncing a media file
        {
            address: '192.168.1.128', hostname: 'MARS', local: 0,
            fppMode: 8, fppModeString: 'remote',
            typeId: 13, type: 'Raspberry Pi 4', model: 'Raspberry Pi 4 Model B Rev 1.1',
            version: '10.x-feature-rgb-output-12-gccc3333dd', majorVersion: 10, minorVersion: 1000,
            uuid: 'M1-100000008c94a071', channelRanges: '0-7',
            channelInputsEnabled: false, channelOutputsEnabled: false,
            multiSyncCapable: 0, multisync: false,
            lastSeen: 1780746608, lastSeenStr: '2026-06-06 06:50:08', HostDescription: '',
        },
        // VENUS — Pi 3 B, player+multisync (multisync:true means mode shows "Player w/ Multisync"),
        //         v9.5-stable branch, behind (update available), green background, HostDescription
        {
            address: '192.168.1.137', hostname: 'VENUS', local: 0,
            fppMode: 2, fppModeString: 'player',
            typeId: 8, type: 'Raspberry Pi 3 B', model: 'Raspberry Pi 3 Model B',
            version: '9.5-stable-7-geee5555ff', majorVersion: 9, minorVersion: 5,
            uuid: 'M1-000000000b08b531', channelRanges: '0-41541',
            channelInputsEnabled: true, channelOutputsEnabled: true,
            multiSyncCapable: 0, multisync: true,
            lastSeen: 1780746610, lastSeenStr: '2026-06-06 06:50:10',
            HostDescription: 'Living Room Display',
        },
        // MERCURY (5a) — eth0 IP, primary entry for polling; develop branch, up-to-date
        {
            address: '192.168.50.1', hostname: 'MERCURY', local: 0,
            fppMode: 2, fppModeString: 'player',
            typeId: 13, type: 'Raspberry Pi 4', model: 'Raspberry Pi 4 Model B Rev 1.2',
            version: '10.x-develop-5-gaaa1111bb', majorVersion: 10, minorVersion: 1000,
            uuid: 'M1-10000000efef343b', channelRanges: '0-8160',
            channelInputsEnabled: false, channelOutputsEnabled: false,
            multiSyncCapable: 1, multisync: false,
            lastSeen: 1780745962, lastSeenStr: '2026-06-06 07:39:22', HostDescription: '',
        },
        // MERCURY (5b) — wlan0 IP, same uuid as 5a.
        //   parseFPPSystems() deduplicates on uuid, merges this entry into the 5a row,
        //   appending the wlan0 IP as a secondary address in the same table cell.
        {
            address: '192.168.50.100', hostname: 'MERCURY', local: 0,
            fppMode: 2, fppModeString: 'player',
            typeId: 13, type: 'Raspberry Pi 4', model: 'Raspberry Pi 4 Model B Rev 1.2',
            version: '10.x-develop-5-gaaa1111bb', majorVersion: 10, minorVersion: 1000,
            uuid: 'M1-10000000efef343b', channelRanges: '0-8160',
            channelInputsEnabled: false, channelOutputsEnabled: false,
            multiSyncCapable: 1, multisync: false,
            lastSeen: 1780745962, lastSeenStr: '2026-06-06 07:39:22', HostDescription: '',
        },
        // PLUTO — typeId 0 is NOT in the FPP range (1-127), so parseFPPSystems() never
        //         adds it to any poll array.  The row stays at "Last Seen" permanently.
        {
            address: '192.168.1.132', hostname: 'PLUTO', local: 0,
            fppMode: 0, fppModeString: 'unknown',
            typeId: 0, type: 'Unknown System Type', model: 'Unknown',
            version: 'Unknown', majorVersion: 0, minorVersion: 0,
            uuid: '', channelRanges: '0-0',
            channelInputsEnabled: false, channelOutputsEnabled: false,
            multiSyncCapable: 0, multisync: false,
            lastSeen: 1780746608, lastSeenStr: '2026-06-06 06:50:08', HostDescription: '',
        },
    ],
};

// Status responses keyed by IP.  Only IPs in the FPP typeId range (1-127) are polled.
// JUPITER (13), SATURN (70), MARS (13), VENUS (8), MERCURY (13) all qualify.
// All five arrive in one request: api/system/status?type=FPP&ip[]=…&ip[]=…
// PLUTO (typeId 0) never appears here.
const MOCK_STATUS: Record<string, unknown> = {
    // JUPITER — idle, channelInputsEnabled, master branch, up-to-date (local=remote git hash)
    '192.168.0.13': {
        status_name: 'idle', mode: 2, mode_name: 'player',
        channelOutputsEnabled: false, channelInputsEnabled: true,
        current_sequence: '', current_song: '', time_elapsed: '00:00',
        rebootFlag: 0, restartFlag: 0, warnings: [], pluginHeaderIndicators: [],
        wifi: [{ interface: 'wlan0', link: 0, level: 0, unit: 'dBm', noise: 0, desc: 'weak' }],
        interfaces: [
            {
                ifname: 'eth0', flags: ['UP'], operstate: 'UP',
                addr_info: [{ family: 'inet', local: '192.168.0.13' }],
            },
            {
                // wlan0 present but link=0 → no wifi icon rendered for JUPITER
                ifname: 'wlan0', flags: ['BROADCAST'], addr_info: [],
                wifi: { interface: 'wlan0', link: 0, level: 0, unit: 'dBm', noise: 0, desc: 'weak' },
            },
        ],
        advancedView: {
            Platform: 'Raspberry Pi', Variant: 'Pi 4',
            SubPlatform: 'Raspberry Pi 4 Model B Rev 1.1',
            backgroundColor: '', Branch: 'master',
            LocalGitVersion: 'aaa1111bb', RemoteGitVersion: 'aaa1111bb',
            OSVersion: 'v2026-05 (32bit)',
            Utilization: {
                CPU: 0.32, Memory: 17.2, Uptime: '11 days 12:25',
                Disk: { Media: { Free: 52979191808, Total: 62314651648 } },
            },
            IPs: ['192.168.0.13'], HostDescription: '',
        },
    },
    // SATURN — idle, both channel directions enabled, master branch, up-to-date,
    //          has a warning (e1.31 target unreachable) that should appear in the warning row
    '192.168.0.14': {
        status_name: 'idle', mode: 2, mode_name: 'player',
        channelOutputsEnabled: true, channelInputsEnabled: true,
        current_sequence: '', current_song: '', time_elapsed: '00:00',
        rebootFlag: 0, restartFlag: 0,
        warnings: ['Cannot reach e1.31 channel data target 10.0.0.99 on Ethernet'],
        pluginHeaderIndicators: [],
        wifi: [{ interface: 'wlan0', link: 0, level: 0, unit: 'dBm', noise: 0, desc: 'weak' }],
        interfaces: [
            {
                ifname: 'eth0', flags: ['UP'], operstate: 'UP',
                addr_info: [{ family: 'inet', local: '192.168.0.14' }],
            },
        ],
        advancedView: {
            Platform: 'BeagleBone Black', Variant: 'SanCloud BeagleBone Enhanced',
            SubPlatform: 'SanCloud BeagleBone Enhanced',
            backgroundColor: '', Branch: 'master',
            LocalGitVersion: 'aaa1111bb', RemoteGitVersion: 'aaa1111bb',
            OSVersion: 'v2026-05 (32bit)',
            Utilization: {
                CPU: 0.79, Memory: 24.6, Uptime: '17 days 10:59',
                Disk: { Media: { Free: 2502782976, Total: 7700291584 } },
            },
            IPs: ['192.168.0.14'], HostDescription: '',
        },
    },
    // MARS — status_name 'idle' + mode_name 'remote' + media_filename set
    //        → getFPPSystemStatus() renders "Syncing:<br>clip.mp4" in the status cell.
    //        Purple background (backgroundColor: '800080'); feature branch; behind (local≠remote).
    '192.168.1.128': {
        status_name: 'idle', mode: 8, mode_name: 'remote',
        channelOutputsEnabled: false, channelInputsEnabled: false,
        current_sequence: '', current_song: 'clip.mp4', time_elapsed: '00:12',
        sequence_filename: '', media_filename: 'clip.mp4',
        rebootFlag: 0, restartFlag: 0, warnings: [], pluginHeaderIndicators: [],
        wifi: [{ interface: 'wlan0', link: 0, level: 0, unit: 'dBm', noise: 0, desc: 'weak' }],
        interfaces: [
            {
                ifname: 'eth0', flags: ['UP'], operstate: 'UP',
                addr_info: [{ family: 'inet', local: '192.168.1.128' }],
            },
        ],
        advancedView: {
            Platform: 'Raspberry Pi', Variant: 'Pi 4',
            SubPlatform: 'Raspberry Pi 4 Model B Rev 1.1',
            backgroundColor: '800080',
            Branch: 'feature-rgb-output',
            LocalGitVersion: 'ccc3333dd', RemoteGitVersion: 'aaa1111bb',
            OSVersion: 'v2026-05 (32bit)',
            Utilization: {
                CPU: 3.93, Memory: 8.0, Uptime: '2 days 11:28',
                Disk: { Media: { Free: 49898205184, Total: 62603276288 } },
            },
            IPs: ['192.168.1.128'], HostDescription: '',
        },
    },
    // VENUS — player with multisync:true.  getFullMode() checks data.multisync from the
    //         STATUS response (not multiSyncSystems).  multisync:true + mode:2 → "Player w/ Multisync".
    //         Green background; v9.5-stable branch; behind.
    //         HostDescription set → appears in <small class="hostDescriptionSM">.
    '192.168.1.137': {
        status_name: 'idle', mode: 2, mode_name: 'player', multisync: true,
        channelOutputsEnabled: true, channelInputsEnabled: true,
        current_sequence: '', current_song: '', time_elapsed: '00:00',
        rebootFlag: 0, restartFlag: 0, warnings: [], pluginHeaderIndicators: [],
        wifi: [{ interface: 'wlan0', link: 0, level: 0, unit: 'dBm', noise: 0, desc: 'weak' }],
        interfaces: [
            {
                ifname: 'eth0', flags: ['UP'], operstate: 'UP',
                addr_info: [{ family: 'inet', local: '192.168.1.137' }],
            },
        ],
        advancedView: {
            Platform: 'Raspberry Pi', Variant: 'Pi 3 Model B',
            SubPlatform: 'Raspberry Pi 3 Model B Rev 1.2',
            backgroundColor: '008000',
            Branch: 'v9.5-stable',
            LocalGitVersion: 'eee5555ff', RemoteGitVersion: 'aaa1111bb',
            OSVersion: 'v2026-05 (32bit)',
            Utilization: {
                CPU: 0.16, Memory: 22.2, Uptime: '2 days 11:27',
                Disk: { Media: { Free: 51932594176, Total: 62305030144 } },
            },
            IPs: ['192.168.1.137'], HostDescription: 'Living Room Display',
        },
    },
    // MERCURY — polled via eth0 (192.168.50.1); wlan0 (192.168.50.100) has link=47 (active).
    //   getFPPSystemStatus() finds the wlan0 interface with link>0 and inserts a wifi icon
    //   immediately after the 192.168.50.100 IP link in the IP address cell.
    //   develop branch; up-to-date (local=remote git hash).
    '192.168.50.1': {
        status_name: 'idle', mode: 2, mode_name: 'player',
        channelOutputsEnabled: false, channelInputsEnabled: false,
        current_sequence: '', current_song: '', time_elapsed: '00:00',
        rebootFlag: 0, restartFlag: 0, warnings: [], pluginHeaderIndicators: [],
        wifi: [{ interface: 'wlan0', link: 47, level: -63, unit: 'dBm', noise: -256, desc: 'good', pct: 67 }],
        interfaces: [
            {
                ifname: 'eth0', flags: ['UP'], operstate: 'UP',
                addr_info: [{ family: 'inet', local: '192.168.50.1' }],
            },
            {
                // wlan0 with link=47 → wifi icon rendered next to 192.168.50.100
                ifname: 'wlan0', flags: ['UP'], operstate: 'UP',
                addr_info: [{ family: 'inet', local: '192.168.50.100' }],
                wifi: { interface: 'wlan0', link: 47, level: -63, unit: 'dBm', noise: -256, desc: 'good', pct: 67 },
            },
        ],
        advancedView: {
            Platform: 'Raspberry Pi', Variant: 'Pi 4',
            SubPlatform: 'Raspberry Pi 4 Model B Rev 1.2',
            backgroundColor: '', Branch: 'develop',
            LocalGitVersion: 'aaa1111bb', RemoteGitVersion: 'aaa1111bb',
            OSVersion: 'v2026-05 (32bit)',
            Utilization: {
                CPU: 0.16, Memory: 5.75, Uptime: '38 days 12:12',
                Disk: { Media: { Free: 1384562688, Total: 7315890176 } },
            },
            // IPs includes both addresses; getFPPSystemStatus() uses this to detect that
            // 192.168.50.100 is already visible in the row and adds the wifi icon there.
            IPs: ['192.168.50.1', '192.168.50.100'], HostDescription: '',
        },
    },
    // PLUTO is never polled (typeId 0 not in FPP range 1-127), so no entry is needed here.
};

// ============================================================
// Helpers
// ============================================================

/**
 * Register all API route stubs.
 *
 * Must be called before page.goto().  The page fires its first fetch (api/proxies)
 * the instant navigation starts; if intercepts aren't live by then the real Docker
 * response arrives and the mock chain breaks.  test.beforeEach() runs before any
 * navigation, making it the correct registration point.
 */
async function setupMocks(page: Page): Promise<void> {
    // api/proxies is the first link in the initialisation chain:
    //   api/proxies → getFPPSystems() → api/fppd/multiSyncSystems → parseFPPSystems()
    //     → getFPPSystemStatus() → api/system/status
    // Without mocking it the chain still starts (Docker responds), but mocking removes
    // a flakiness source when Docker is under load.
    await page.route('**/api/proxies', route =>
        route.fulfill({ json: [] }));

    await page.route('**/api/git/branches', route =>
        route.fulfill({ json: ['master', 'develop', 'feature-rgb-output'] }));

    await page.route('**/api/fppd/multiSyncSystems', route =>
        route.fulfill({ json: MOCK_SYSTEMS }));

    // One handler covers all type variants (type=FPP, type=WLED, type=Genius, etc.).
    // All five polled mock devices have typeIds in the FPP range (1-127), so they all
    // arrive together in one type=FPP request.  For other device types our handler
    // returns MOCK_STATUS but the pollers find no matching IPs and do nothing.
    await page.route('**/api/system/status**', route =>
        route.fulfill({ json: MOCK_STATUS }));

    // Channel I/O icon fallback path: older FPP systems that don't report
    // channelOutputsEnabled in their status trigger a secondary config fetch.
    await page.route('**/api/channel/output/universeOutputs**', route =>
        route.fulfill({ json: { channelOutputs: [] } }));
    await page.route('**/api/channel/output/universeInputs**', route =>
        route.fulfill({ json: { channelInputs: [] } }));

    // Local OS upgrade file list shown in the action panel
    await page.route('**/api/files/uploads', route =>
        route.fulfill({ json: { files: [] } }));

    // Absorb settings writes (display order saves, SetRestartFlag, etc.) without errors.
    // Exception: initialSetup-02 must reach the real Docker server so that PHP's
    // menuHead.inc finds $settings['initialSetup-02']='1' and doesn't redirect.
    await page.route('**/api/settings/**', route => {
        if (route.request().url().includes('initialSetup-02')) {
            return route.continue();
        }
        return route.fulfill({ json: {} });
    });
}

/**
 * Navigate to multisync.php and wait for the table to be fully populated.
 *
 * Waits for JUPITER (the local device, always rendered first) to appear, then
 * waits for the initial status poll response so mode/status cells are updated.
 * Call this after setupMocks() — mocks must be live before goto().
 */
async function loadMultisync(page: Page): Promise<void> {
    // menuHead.inc redirects any page to initialSetup.php when $settings['initialSetup-02']
    // is absent or '0'.  Write it to the Docker server before navigating so the PHP
    // renders normally.  This call bypasses our mock (see the initialSetup-02 exception
    // in setupMocks) so it reaches the real FPP settings store.
    await page.request.put('/api/settings/initialSetup-02', {
        data: '"1"',
        headers: { 'Content-Type': 'application/json' },
    });

    // Register the status response watcher before navigation so we don't miss
    // the response that fires during page initialisation.
    const firstStatusResponse = page.waitForResponse(r =>
        r.url().includes('/api/system/status'));

    await page.goto('/multisync.php');

    await page.locator('text=JUPITER').waitFor({ state: 'visible', timeout: 10_000 });

    // Ensure at least one status poll has completed so the mode/status cells
    // are populated before any test assertions run.
    await firstStatusResponse;
}

/**
 * Returns a promise that resolves on the next api/system/status response.
 *
 * Use instead of waitForTimeout() so tests advance deterministically when the poll
 * actually completes, not after a fixed sleep that may be too short under load.
 * Register this BEFORE the action that triggers the poll — otherwise the response
 * can arrive before waitForResponse starts watching and the promise never resolves.
 */
function nextStatusPoll(page: Page): Promise<unknown> {
    return page.waitForResponse(r => r.url().includes('/api/system/status'));
}

/**
 * Returns the hostname text of every visible system row, in DOM order.
 *
 * Why not read .textContent on the whole hostname cell?  The cell has three child
 * nodes: the drag-grip icon span, the hostname span, and a HostDescription <small>.
 * Reading the cell text concatenates them all (e.g. "⠿ VENUS\nLiving Room Display").
 * The hostname span has id="fpp_{ip_dashes}_hostname", so we target it directly.
 */
async function getRowOrder(page: Page): Promise<string[]> {
    return page.locator('#fppSystems tr.systemRow').evaluateAll(rows =>
        rows.map(r => {
            const el = r.querySelector('span[id^="fpp_"]');
            return el ? (el.textContent ?? '').trim() : r.id;
        })
    );
}

// ============================================================
// Test setup
// ============================================================

test.beforeEach(async ({ page }) => {
    // Mocks MUST be registered before page.goto() — see setupMocks() docstring above.
    await setupMocks(page);
});

// ============================================================
// Tests
// ============================================================

test('Multisync page renders correct device rows', async ({ page }) => {
    await loadMultisync(page);

    // MERCURY's two MOCK_SYSTEMS entries share a uuid → parseFPPSystems() deduplicates
    // them into one row, so we expect 6 rows total, not 7.
    await expect(page.locator('#fppSystems tr.systemRow')).toHaveCount(6);

    // JUPITER — local device: hostname span gets class="fw-bold" for local nodes
    await expect(page.locator('span[id^="fpp_"][class*="fw-bold"]')).toContainText('JUPITER');

    // SATURN — warning row must be visible and contain e1.31 text
    const saturnRow = page.locator('tr.systemRow', { hasText: 'SATURN' });
    const saturnId = await saturnRow.getAttribute('id');
    await expect(page.locator(`#${saturnId}_warnings`)).toBeVisible();
    await expect(page.locator(`#${saturnId}_warningCell`)).toContainText('e1.31');

    // MARS — purple background applied via rowStyle() from backgroundColor: '800080'.
    // Browsers normalize hex to rgb(), so compare against rgb(128, 0, 128).
    const marsRow = page.locator('tr.systemRow', { hasText: 'MARS' });
    const marsBg = await marsRow.evaluate(el => (el as HTMLElement).style.background);
    expect(marsBg).toContain('rgb(128, 0, 128)');

    // VENUS — green background; multisync:true → mode cell shows "Player w/ Multisync"
    const venusRow = page.locator('tr.systemRow', { hasText: 'VENUS' });
    const venusBg = await venusRow.evaluate(el => (el as HTMLElement).style.background);
    expect(venusBg).toContain('rgb(0, 128, 0)');
    await expect(venusRow).toContainText('Player w/ Multisync');

    // VENUS — HostDescription "Living Room Display" appears in <small class="hostDescriptionSM">
    await expect(venusRow.locator('small.hostDescriptionSM')).toContainText('Living Room Display');

    // MERCURY — single merged row showing both IPs; wifi icon present for wlan0 (link=47)
    const mercuryRow = page.locator('tr.systemRow', { hasText: 'MERCURY' });
    await expect(mercuryRow).toContainText('192.168.50.1');
    await expect(mercuryRow).toContainText('192.168.50.100');
    await expect(mercuryRow.locator('.wifi-icon')).toBeVisible();

    // PLUTO — typeId 0 is never polled; status cell still shows the initial "Last Seen" text
    const plutoRow = page.locator('tr.systemRow', { hasText: 'PLUTO' });
    await expect(plutoRow).toContainText('Last Seen');
});

test('Platform filter: FPP (BeagleBone) hides Pi rows', async ({ page }) => {
    await loadMultisync(page);

    // Platform filter is a <select> whose options come from window.platformFilterOptions.
    // We identify it by a unique option value that only the Platform select has.
    const platformSelect = page
        .locator('#fppSystemsTable thead select')
        .filter({ has: page.locator('option[value="FPP (BeagleBone)"]') });

    await platformSelect.selectOption({ value: 'FPP (BeagleBone)' });

    // SATURN (typeId 70 = SanCloud BBE, falls in BeagleBone range 65-95) stays visible
    await expect(page.locator('tr.systemRow', { hasText: 'SATURN' })).toBeVisible();

    // All Pi rows (typeId 13 = Raspberry Pi 4, typeId 8 = Pi 3 B) are hidden
    await expect(page.locator('tr.systemRow', { hasText: 'JUPITER' })).not.toBeVisible();
    await expect(page.locator('tr.systemRow', { hasText: 'MARS' })).not.toBeVisible();
    await expect(page.locator('tr.systemRow', { hasText: 'VENUS' })).not.toBeVisible();
    await expect(page.locator('tr.systemRow', { hasText: 'MERCURY' })).not.toBeVisible();
});

test('Filter focus is not stolen by auto-refresh', async ({ page }) => {
    // BUG: bt.initBody() re-creates the filter <select> as a new DOM node every poll.
    // The browser drops focus to <body> because the old node is destroyed.  A user
    // mid-selection of a filter loses their choice.
    // FIX: safeInitBody() saves the focused element's id+value before initBody() and
    // re-applies them after, keeping the selection intact across re-renders.

    await loadMultisync(page);

    // The Mode filter is a <select> with options from window.modeFilterOptions.
    // We identify it by its unique "Remote" option value — the Platform select uses
    // device-class strings ("FPP (All)", "Falcon", etc.) and has no "Remote" option.
    // Bootstrap-table-filter-control does not set data-field on the generated <th>,
    // so position-based selectors are fragile when columns are hidden responsively;
    // filtering by option value is stable regardless of column layout.
    const modeSelect = page
        .locator('#fppSystemsTable thead select')
        .filter({ has: page.locator('option[value="Remote"]') });

    // Register the poll watcher BEFORE enabling auto-refresh so we don't race
    // the immediate RefreshStats() call that autoRefreshToggled() fires.
    const poll1 = nextStatusPoll(page);
    await page.locator('#MultiSyncRefreshStatus').check();
    await poll1;

    // Select "Player" in the Mode filter — exactly the state a user is in when
    // the 2-second timer fires.
    // Note: modeFilterOptions maps 'Player w/ Multisync' → value 'Multisync',
    // 'Player' → value 'Player'.  We use value not label to be unambiguous.
    await modeSelect.selectOption({ value: 'Player' });
    await expect(modeSelect).toHaveValue('Player');

    // Wait for the next scheduled poll (fires ~2 s after poll1 completed).
    // This is the tick that previously stole focus; with safeInitBody it must not.
    const poll2 = nextStatusPoll(page);
    await poll2;

    await expect(modeSelect).toHaveValue('Player');
});

test('Reorder mode: drag order survives auto-refresh', async ({ page }) => {
    // BUG: bt.initBody() re-renders rows from bt.options.data, which retains original
    // insertion order.  jQuery UI sortable only reorders DOM nodes — it does not update
    // bt.options.data.  Every poll therefore reset the table to pre-drag order.
    // FIX: safeInitBody() detects reorderModeActive, reads the current DOM row order,
    // rewrites bt.options.data to match, then calls initBody() — preserving the drag.

    await loadMultisync(page);

    const poll1 = nextStatusPoll(page);
    await page.locator('#MultiSyncRefreshStatus').check();
    await poll1;

    // #reorderModeBtn adds class 'reorder-mode' to the table and calls $tbody.sortable().
    // The .reorder-grip handles are display:none until 'reorder-mode' is present, so we
    // must wait for them to become visible before attempting to drag.
    await page.locator('#reorderModeBtn').click();
    const firstGrip = page.locator('#fppSystems tr.systemRow .reorder-grip').first();
    await firstGrip.waitFor({ state: 'visible' });

    const before = await getRowOrder(page);

    // dragTo() simulates mousedown → mousemove → mouseup, which is exactly what jQuery
    // UI sortable listens for.  Dragging grip[0] onto grip[1] triggers a row swap
    // (jQuery UI 'pointer' tolerance fires when the dragged item overlaps the midpoint
    // of an adjacent sibling).
    const grips = page.locator('#fppSystems tr.systemRow .reorder-grip');
    await grips.nth(0).dragTo(grips.nth(1));

    const afterDrag = await getRowOrder(page);
    // Guard: confirm the drag actually changed the order before waiting for the refresh.
    expect(afterDrag).not.toEqual(before);

    // Wait for the next scheduled poll — the tick that previously reset the order.
    const poll2 = nextStatusPoll(page);
    await poll2;

    const afterRefresh = await getRowOrder(page);
    expect(afterRefresh).toEqual(afterDrag);
});

test('Checked checkbox survives auto-refresh', async ({ page }) => {
    // BUG: The checkbox HTML is a static string with no "checked" attribute:
    //   "<input type='checkbox' class='…multisyncRowCheckbox' name='{ip}'>"
    // bt.initBody() renders this string verbatim every poll, always producing an
    // unchecked box — discarding whatever the user had selected.
    // FIX: safeInitBody() snapshots checked states (keyed by name=ip) before
    // initBody() and re-applies them to the freshly rendered checkboxes after.

    await loadMultisync(page);

    // .multisyncRowCheckbox is generated for FPP devices where majorVersion >= 4.
    // All five FPP mock devices qualify (all have majorVersion 9 or 10).
    // PLUTO (typeId 0) has no checkbox.
    // Wait for visibility: the checkbox only appears after bootstrapTable('load', data)
    // inside parseFPPSystems() completes.
    const checkbox = page.locator('input.multisyncRowCheckbox').first();
    await checkbox.waitFor({ state: 'visible' });

    // Enable auto-refresh and let the immediate poll settle before checking the box,
    // ensuring the checkbox node we interact with is from the post-status-poll DOM.
    const poll1 = nextStatusPoll(page);
    await page.locator('#MultiSyncRefreshStatus').check();
    await poll1;

    await checkbox.check();
    await expect(checkbox).toBeChecked();

    // Wait for the next scheduled poll — the tick that previously cleared the box.
    const poll2 = nextStatusPoll(page);
    await poll2;

    await expect(checkbox).toBeChecked();
});

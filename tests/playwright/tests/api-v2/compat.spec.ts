import { test, expect } from '@playwright/test';
import { testBothVerbVersions, V1, V2 } from '../../fixtures/version';

test.describe('api compatibility', () => {
  testBothVerbVersions(
    '/system/reboot',
    { method: 'GET', path: '/system/reboot', tags: ['@v1', '@destructive'] },
    { method: 'POST', path: '/system/reboot', tags: ['@v2', '@destructive'] },
    async (res) => {
      expect([200, 202]).toContain(res.status());
    }
  );

  test('GET /remoteAction [v1]', { tag: ['@v1', '@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    test.skip(true, 'Requires a reachable remote FPP instance at a known IP');
    await request.get(`${V1}/remoteAction?ip=192.0.2.1&action=reboot`);
  });

  test('POST /remoteAction [v2]', { tag: ['@v2', '@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    test.skip(true, 'Requires a reachable remote FPP instance at a known IP');
    await request.post(`${V2}/remoteAction`, {
      data: { ip: '192.0.2.1', action: 'reboot' },
    });
  });
});

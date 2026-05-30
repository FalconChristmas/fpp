import { test, expect } from '@playwright/test';

const V2 = '/api/v2';

test.describe('email', () => {

  test('POST /email/configure', { tag: ['@email', '@notest'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'EMAIL' });
    test.info().annotations.push({ type: 'tier', description: 'NOTEST' });
    // Requires: valid SMTP config values; this will mutate the email configuration on the device
    test.skip(true, 'No test written. Requires valid SMTP credentials and mail server config');
  });

  test('POST /email/test', { tag: ['@email', '@notest'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'EMAIL' });
    test.info().annotations.push({ type: 'tier', description: 'NOTEST' });
    // This sends an actual email to the configured recipient; only run with a real mail config
    test.skip(true, 'No test written. Sends a real email — requires configured email settings on the device');
  });

});

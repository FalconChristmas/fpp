import { test, expect } from '@playwright/test';

const V2 = '/api/v2';

test.describe('file', () => {

  test('GET /file/info/:plugin/:ext/**', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: an installed plugin with a known file
    test.skip(true, 'Requires an installed plugin with a known file');
  });

  test('POST /file/onUpload/:ext/**', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: an installed plugin with an onUpload handler for the given extension
    test.skip(true, 'Requires an installed plugin with a file upload handler');
  });

  test('POST /file/move/:fileName', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: a file to exist at a known path; this is tested inline via POST /file/:DirName
    test.skip(true, 'Requires a pre-existing file to move');
  });

  test('POST /file/:DirName/copy/:source/:dest', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Upload a source file, copy it, then clean up both
    const uploadRes = await request.post(`${V2}/file/sequences`, {
      multipart: {
        file: {
          name: 'ci-copy-source.txt',
          mimeType: 'text/plain',
          buffer: Buffer.from('ci test content'),
        },
      },
    });
    expect(uploadRes.status()).toBe(200);

    const copyRes = await request.post(`${V2}/file/sequences/copy/ci-copy-source.txt/ci-copy-dest.txt`);
    expect(copyRes.status()).toBe(200);

    await request.delete(`${V2}/file/sequences/ci-copy-source.txt`);
    await request.delete(`${V2}/file/sequences/ci-copy-dest.txt`);
  });

  test('POST /file/:DirName/rename/:source/:dest', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    const uploadRes = await request.post(`${V2}/file/sequences`, {
      multipart: {
        file: {
          name: 'ci-rename-source.txt',
          mimeType: 'text/plain',
          buffer: Buffer.from('ci test content'),
        },
      },
    });
    expect(uploadRes.status()).toBe(200);

    const renameRes = await request.post(`${V2}/file/sequences/rename/ci-rename-source.txt/ci-rename-dest.txt`);
    expect(renameRes.status()).toBe(200);

    await request.delete(`${V2}/file/sequences/ci-rename-dest.txt`);
  });

  test('GET /file/:DirName/tailfollow/**', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Streaming (chunked transfer) endpoint — meaningful only when consumed as a stream
    test.skip(true, 'Streaming endpoint requires SSE/chunked consumer; not suitable for request fixture');
  });

  test('GET /file/:DirName/**', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Upload a file first, then download it
    const content = 'ci get test\n';
    const uploadRes = await request.post(`${V2}/file/sequences`, {
      multipart: {
        file: {
          name: 'ci-get-test.txt',
          mimeType: 'text/plain',
          buffer: Buffer.from(content),
        },
      },
    });
    expect(uploadRes.status()).toBe(200);

    const getRes = await request.get(`${V2}/file/sequences/ci-get-test.txt`);
    expect(getRes.status()).toBe(200);

    await request.delete(`${V2}/file/sequences/ci-get-test.txt`);
  });

  test('DELETE /file/:DirName/**', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    const uploadRes = await request.post(`${V2}/file/sequences`, {
      multipart: {
        file: {
          name: 'ci-delete-test.txt',
          mimeType: 'text/plain',
          buffer: Buffer.from('ci delete test\n'),
        },
      },
    });
    expect(uploadRes.status()).toBe(200);

    const delRes = await request.delete(`${V2}/file/sequences/ci-delete-test.txt`);
    expect(delRes.status()).toBe(200);
  });

  test('POST /file/:DirName — upload', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    const uploadRes = await request.post(`${V2}/file/sequences`, {
      multipart: {
        file: {
          name: 'ci-upload-test.txt',
          mimeType: 'text/plain',
          buffer: Buffer.from('ci upload test\n'),
        },
      },
    });
    expect(uploadRes.status()).toBe(200);

    await request.delete(`${V2}/file/sequences/ci-upload-test.txt`);
  });

  test('POST /file/:DirName/:Name — upload with explicit name', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    const uploadRes = await request.post(`${V2}/file/sequences/ci-named-upload.txt`, {
      multipart: {
        file: {
          name: 'ci-named-upload.txt',
          mimeType: 'text/plain',
          buffer: Buffer.from('ci named upload test\n'),
        },
      },
    });
    expect(uploadRes.status()).toBe(200);

    await request.delete(`${V2}/file/sequences/ci-named-upload.txt`);
  });

});

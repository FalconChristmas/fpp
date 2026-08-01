import { APIRequestContext, APIResponse, test } from '@playwright/test';

export const V1 = '/api';
export const V2 = '/api/v2';

type Method = 'GET' | 'POST' | 'PUT' | 'PATCH' | 'DELETE';

type VersionRoute = {
  method: Method;
  path: string;
  tags: string[];
};

type ResponseAssert = (response: APIResponse) => Promise<void>;

async function dispatchRequest(
  request: APIRequestContext,
  route: VersionRoute,
  basePath: string
): Promise<APIResponse> {
  const method = route.method.toLowerCase() as Lowercase<Method>;
  return request[method](`${basePath}${route.path}`);
}

export function testBothVerbVersions(
  label: string,
  v1Route: VersionRoute,
  v2Route: VersionRoute,
  assertResponse: ResponseAssert
): void {
  test(`${label} [v1]`, { tag: v1Route.tags }, async ({ request }) => {
    const response = await dispatchRequest(request, v1Route, V1);
    await assertResponse(response);
  });

  test(`${label} [v2]`, { tag: v2Route.tags }, async ({ request }) => {
    const response = await dispatchRequest(request, v2Route, V2);
    await assertResponse(response);
  });
}

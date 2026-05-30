<?php

/**
 * Parse the HTTP request body as JSON.
 *
 * Reads `php://input`, decodes it as a JSON object, and returns the result as
 * an associative array. When parsing fails and `$required` is `true`, the
 * function sends a `400 Bad Request` response and halts immediately so callers
 * do not need to repeat the error-handling boilerplate. When `$required` is
 * `false`, the function returns `null` on failure and lets the caller decide
 * how to proceed.
 *
 * @param bool $required Halt with 400 when the body is absent or invalid JSON.
 * @return array|null    Decoded JSON as an associative array, or null on failure.
 */
function getJsonBody(bool $required = true): ?array
{
    $raw = file_get_contents('php://input');

    if ($raw === false || trim($raw) === '') {
        if ($required) {
            http_response_code(400);
            echo json_encode(['status' => 'error', 'message' => 'Request body is required']);
            exit;
        }
        return null;
    }

    $data = json_decode($raw, true);

    if ($data === null && json_last_error() !== JSON_ERROR_NONE) {
        if ($required) {
            http_response_code(400);
            echo json_encode(['status' => 'error', 'message' => 'Invalid JSON: ' . json_last_error_msg()]);
            exit;
        }
        return null;
    }

    return $data;
}

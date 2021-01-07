<?php
declare(strict_types=1);

/**
 * Based on Laravel Helpers
 */

if (!function_exists('array_add')) {
    /**
     * Add an element to an array using "dot" notation if it doesn't exist.
     *
     * @param array $array
     * @param string $key
     * @param mixed $value
     * @return array
     */
    function array_add(array $array, string $key, $value): array
    {
        if (is_null(array_get($array, $key))) {
            array_set($array, $key, $value);
        }

        return $array;
    }
}

if (!function_exists('array_build')) {
    /**
     * Build a new array using a callback.
     *
     * @param array $array
     * @param callable $callback
     * @return array
     */
    function array_build(array $array, callable $callback): array
    {
        $results = [];

        foreach ($array as $key => $value) {
            list($innerKey, $innerValue) = call_user_func($callback, $key, $value);

            $results[$innerKey] = $innerValue;
        }

        return $results;
    }
}

if (!function_exists('array_collapse')) {
    /**
     * Collapse an array of arrays into a single array.
     *
     * @param array $array
     * @return array
     */
    function array_collapse(array $array): array
    {
        $results = [];

        foreach ($array as $values) {
            $results = array_merge($results, $values);
        }

        return $results;
    }
}

if (!function_exists('array_divide')) {
    /**
     * Divide an array into two arrays. One with keys and the other with values.
     *
     * @param array $array
     * @return array
     */
    function array_divide(array $array): array
    {
        return [array_keys($array), array_values($array)];
    }
}

if (!function_exists('array_dot')) {
    /**
     * Flatten a multi-dimensional associative array with dots.
     *
     * @param array $array
     * @param string $prepend
     * @return array
     */
    function array_dot(array $array, string $prepend = ''): array
    {
        $results = [];

        foreach ($array as $key => $value) {
            if (is_array($value)) {
                $results = array_merge($results, array_dot($value, $prepend . $key . '.'));
            } else {
                $results[$prepend . $key] = $value;
            }
        }

        return $results;
    }
}

if (!function_exists('array_except')) {
    /**
     * Get all of the given array except for a specified array of items.
     *
     * @param array $array
     * @param array|string $keys
     * @return array
     */
    function array_except(array $array, $keys): array
    {
        array_forget($array, $keys);

        return $array;
    }
}

if (!function_exists('array_fetch')) {
    /**
     * Fetch a flattened array of a nested array element.
     *
     * @param array $array
     * @param string $key
     * @return array
     */
    function array_fetch(array $array, string $key): array
    {
        $results = [];

        foreach (explode('.', $key) as $segment) {
            $results = [];

            foreach ($array as $value) {
                if (array_key_exists($segment, $value = (array)$value)) {
                    $results[] = $value[$segment];
                }
            }

            $array = array_values($results);
        }

        return array_values($results);
    }
}

if (!function_exists('array_first')) {
    /**
     * Return the first element in an array passing a given truth test.
     *
     * @param array $array
     * @param callable $callback
     * @param mixed $default
     * @return mixed
     */
    function array_first(array $array, callable $callback, $default = null)
    {
        foreach ($array as $key => $value) {
            if ($callback($value, $key)) {
                return $value;
            }
        }

        return $default;
    }
}

if (!function_exists('array_last')) {
    /**
     * Return the last element in an array passing a given truth test.
     *
     * @param array $array
     * @param callable $callback
     * @param mixed $default
     * @return mixed
     */
    function array_last(array $array, callable $callback, $default = null)
    {
        return array_first(array_reverse($array), $callback, $default);
    }
}

if (!function_exists('array_flatten')) {
    /**
     * Flatten a multi-dimensional array into a single level.
     *
     * @param array $array
     * @return array
     */
    function array_flatten(array $array): array
    {
        $return = [];

        array_walk_recursive(
            $array,
            function ($x) use (&$return) {
                $return[] = $x;
            }
        );

        return $return;
    }
}

if (!function_exists('array_forget')) {
    /**
     * Remove one or many array items from a given array using "dot" notation.
     *
     * @param array $array
     * @param array|string $keys
     * @return void
     */
    function array_forget(array &$array, $keys): void
    {
        $original =& $array;

        foreach ((array)$keys as $key) {
            $parts = explode('.', $key);

            while (count($parts) > 1) {
                $part = array_shift($parts);

                if (isset($array[$part]) && is_array($array[$part])) {
                    $array =& $array[$part];
                }
            }

            unset($array[array_shift($parts)]);

            // clean up after each pass
            $array =& $original;
        }
    }
}

if (!function_exists('array_get')) {
    /**
     * Get an item from an array using "dot" notation.
     *
     * @param array $array
     * @param string $key
     * @param mixed $default
     * @return mixed
     */
    function array_get(array $array, string $key, $default = null)
    {
        if (is_null($key)) {
            return $array;
        }

        if (isset($array[$key])) {
            return $array[$key];
        }

        foreach (explode('.', $key) as $segment) {
            if (!is_array($array) || !array_key_exists($segment, $array)) {
                unset($array, $key);

                return $default;
            }

            $array = $array[$segment];
        }

        unset($key, $default);

        return $array;
    }
}

if (!function_exists('array_has')) {
    /**
     * Check if an item exists in an array using "dot" notation.
     *
     * @param array $array
     * @param string $key
     * @return bool
     */
    function array_has(array $array, string $key): bool
    {
        if (empty($array) || is_null($key)) {
            return false;
        }

        if (array_key_exists($key, $array)) {
            return true;
        }

        foreach (explode('.', $key) as $segment) {
            if (!is_array($array) || !array_key_exists($segment, $array)) {
                unset($array, $key);

                return false;
            }

            $array = $array[$segment];
        }

        unset($array, $key);

        return true;
    }
}

if (!function_exists('array_only')) {
    /**
     * Get a subset of the items from the given array.
     *
     * @param array $array
     * @param array|string $keys
     * @return array
     */
    function array_only(array $array, $keys): array
    {
        return array_intersect_key($array, array_flip((array)$keys));
    }
}

if (!function_exists('array_only_with_default')) {
    /**
     * Get a subset of the items from the given array filling it with default value on non existing keys.
     *
     * @param array $array
     * @param array $keys
     * @param mixed $default
     * @return array
     */
    function array_only_with_default(array $array, array $keys, $default = null): array
    {
        $defaultArray = array_combine($keys, array_fill(0, count($keys), $default));

        return array_merge($defaultArray, array_only($array, $keys));
    }
}

if (!function_exists('array_wrap')) {
    /**
     * If the given value is not an array and not null, wrap it in one.
     *
     * @param mixed $value
     * @return array
     */
    function array_wrap($value): array
    {
        if (is_null($value)) {
            return [];
        }

        return is_array($value) ? $value : [$value];
    }
}

if (!function_exists('array_fill_default')) {
    /**
     * Fill a nested array with default value when missing
     *
     * @param array $array
     * @param array $default
     * @return array
     */
    function array_fill_default(array &$array, array $default): array
    {
        foreach ($array as &$item) {
            $item = is_array($item) ? array_replace_recursive($default, $item) : $default;
        }

        return $array;
    }
}

if (!function_exists('array_pluck')) {
    /**
     * Pluck an array of values from an array.
     *
     * @param array $array
     * @param string $value
     * @param string $key
     * @return array
     */
    function array_pluck(array $array, string $value, string $key = null): array
    {
        $results = [];

        foreach ($array as $item) {
            $itemValue = data_get($item, $value);

            if (is_null($key)) {
                $results[] = $itemValue;
            } else {
                $itemKey = data_get($item, $key);

                $results[$itemKey] = $itemValue;
            }
        }

        return $results;
    }
}

if (!function_exists('array_pull')) {
    /**
     * Get a value from the array, and remove it.
     *
     * @param array $array
     * @param string $key
     * @param mixed $default
     * @return mixed
     */
    function array_pull(array &$array, string $key, $default = null)
    {
        $value = array_get($array, $key, $default);

        array_forget($array, $key);

        return $value;
    }
}

if (!function_exists('array_set')) {
    /**
     * Set an array item to a given value using "dot" notation.
     *
     * If no key is given to the method, the entire array will be replaced.
     *
     * @param array $array
     * @param string $key
     * @param mixed $value
     * @return array
     */
    function array_set(array &$array, string $key, $value): array
    {
        if (is_null($key)) {
            return $array = $value;
        }

        $keys = explode('.', $key);

        while (count($keys) > 1) {
            $key = array_shift($keys);

            if (!isset($array[$key]) || !is_array($array[$key])) {
                $array[$key] = [];
            }

            $array =& $array[$key];
        }

        $array[array_shift($keys)] = $value;

        unset($key, $keys, $value);

        return $array;
    }
}

if (!function_exists('array_where')) {
    /**
     * Filter the array using the given callback.
     *
     * @param array $array
     * @param callable $callback
     * @return array
     */
    function array_where(array $array, callable $callback): array
    {
        $filtered = [];

        foreach ($array as $key => $value) {
            if (call_user_func($callback, $key, $value)) {
                $filtered[$key] = $value;
            }
        }

        return $filtered;
    }
}

if (!function_exists('data_get')) {
    /**
     * Get an item from an array or object using "dot" notation.
     *
     * @param mixed $target
     * @param string $key
     * @param mixed $default
     * @return mixed
     */
    function data_get($target, string $key, $default = null)
    {
        if (is_null($key)) {
            return $target;
        }

        foreach (explode('.', $key) as $segment) {
            if (is_array($target)) {
                if (!array_key_exists($segment, $target)) {
                    return $default;
                }

                $target = $target[$segment];
            } elseif ($target instanceof ArrayAccess) {
                if (!isset($target[$segment])) {
                    return $default;
                }

                $target = $target[$segment];
            } elseif (is_object($target)) {
                if (!isset($target->{$segment})) {
                    return $default;
                }

                $target = $target->{$segment};
            } else {
                return $default;
            }
        }

        return $target;
    }
}

if (!function_exists('head')) {
    /**
     * Get the first element of an array. Useful for method chaining.
     *
     * @param array $array
     * @return mixed
     */
    function head(array $array)
    {
        return reset($array);
    }
}

if (!function_exists('last')) {
    /**
     * Get the last element from an array.
     *
     * @param array $array
     * @return mixed
     */
    function last(array $array)
    {
        return end($array);
    }
}

if (!function_exists('array_random')) {
    /**
     * Get a random element of an array
     *
     * @param array $array Array to get random element
     * @return mixed
     */
    function array_random(array $array)
    {
        if ($array === []) {
            return null;
        }

        return $array[array_rand($array)];
    }
}

if (!function_exists('array_undot')) {
    /**
     * Convert a flattened dotted array to its array equivalence with nested levels represented by dots.
     *
     * @param array $dottedArray
     * @return array
     */
    function array_undot(array $dottedArray): array
    {
        $out = [];

        foreach ($dottedArray as $dotKey => $value) {
            array_set($out, $dotKey, $value);
        }

        return $out;
    }
}

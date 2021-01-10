<?php

namespace App\Kernel\Abstracts;

use Cocur\Slugify\Slugify;
use Twig\Extension\AbstractExtension;
use Twig\Extension\GlobalsInterface;
use Twig\TwigFilter;
use Twig\TwigFunction;

class TwigViewAbstract extends AbstractExtension implements GlobalsInterface
{
    /**
     * @var Slugify
     */
    private $slugifier;

    /**
     * TwigViewAbstract constructor.
     */
    public function __construct()
    {
        $this->slugifier = container('slugifier');
    }

    public function getName(): string
    {
        return 'fpp-functions';
    }

    public function getGlobals(): array
    {
        return [
            'app' => [
                'environment' => env('APP_ENV'),
                'url' => env('APP_URL'),
            ],
        ];
    }

    /**
     * @inheritdoc
     */
    public function getFunctions()
    {
        return [
            new TwigFunction('render_classes', [$this, 'renderClassesFunction']),
            new TwigFunction('render_attrs', [$this, 'renderAttributesFunction'], ['is_safe' => ['html']]),
            new TwigFunction('render_element_classes', [$this, 'renderElementClassesFunction']),
            new TwigFunction('render_stimulus_action', [$this, 'renderStimulusActionFunction']),
            new TwigFunction('render_stimulus_target', [$this, 'renderStimulusTargetFunction']),
            new TwigFunction('merge_component_options', [$this, 'mergeComponentOptionsFunction']),
            new TwigFunction('merge_modifiers', [$this, 'mergeModifiers']),
        ];
    }

    /**
     * @inheritdoc
     */
    public function getFilters()
    {
        return [
            new TwigFilter('kebab', [$this, 'kebabCaseFilter'], ['is_safe' => ['html']]),
            new TwigFilter('snake', [$this, 'snakeCaseFilter'], ['is_safe' => ['html']]),
        ];
    }

    /**
     * Renders given element attributes as a flattened string.
     *
     * @param array $attrs
     *
     * @return string
     */
    public function renderAttributesFunction(array $attrs)
    {
        $pairs = [];

        foreach ($attrs as $name => $value) {
            $pairs[] = sprintf('%s="%s"', $name, $value);
        }

        return implode(' ', $pairs);
    }

    /**
     * Creates a string of css class attribute with its modifiers.
     *
     * @param string|null $base
     * @param array|null $modifiers
     * @param string|null $extraClass
     * @param bool $onlyModifiers
     *
     * @return string
     */
    public function renderClassesFunction($base = null, array $modifiers = null, string $extraClass = null, bool $onlyModifiers = false): string
    {
        $classes = [$base];

        if ($onlyModifiers) {
            $classes = [];
        }

        if (null === $modifiers) {
            return $base;
        }

        foreach ($modifiers as $modifier) {
            if (null === $modifier) {
                break;
            }

            $classes[] = sprintf('%s--%s', $base, $modifier);
        }

        if (null !== $extraClass) {
            $classes[] = $extraClass;
        }

        return trim(implode(' ', $classes));
    }

    /**
     * Merges an component options array with its default options
     *
     * @param array|null $options
     * @param array|null $defaults
     * @param array|null $modifiers
     *
     * @return array
     */
    public function mergeComponentOptionsFunction(array $options = null, array $defaults = null, array $modifiers = null): array
    {
        if (null === $options) {
            $options = [];
        }

        if (null === $defaults) {
            $defaults = [];
        }

        // all component modifiers should be merged with modifiers provided via options
        if (null !== $modifiers) {
            $options = $this->mergeModifiersFunction($options, $modifiers);
        }

        return array_merge($defaults, $options);
    }

    /**
     * Merges an array of css class modifiers with new modifiers
     *
     * @param array $attrs
     * @param array|null $modifiers
     *
     * @return array
     */
    public function mergeModifiersFunction(array $attrs, array $modifiers = []): array
    {
        if (null === $modifiers) {
            return $attrs;
        }

        if (!array_key_exists('modifiers', $attrs)) {
            $attrs['modifiers'] = [];
        }

        $originalModifiers = $attrs['modifiers'];

        if (!is_array($originalModifiers)) {
            $originalModifiers = [$originalModifiers];
        }

        $modifiers = array_merge($originalModifiers, $modifiers);

        return array_merge($attrs, ['modifiers' => $modifiers]);
    }

    /**
     * Creates a string of css classnames with its modifiers for a component block element.
     *
     * @param string $base
     * @param string $element
     * @param array|null $modifiers
     * @param string|null $extraClass
     * @param bool $onlyModifiers
     *
     * @return string
     */
    public function renderElementClassesFunction(
        string $base,
        string $element,
        array $modifiers = null,
        string $extraClass = null,
        bool $onlyModifiers = false
    ): string
    {
        $base = sprintf('%s__%s', $base, $element);

        return $this->renderClassesFunction($base, $modifiers, $extraClass, $onlyModifiers);
    }

    /**
     * Creates a string for a stimulus controller target.
     *
     * @param string $controller
     * @param string $target
     *
     * @return string
     */
    public function renderStimulusTargetFunction($controller, $target): string
    {
        return sprintf('%s.%s', $controller, $target);
    }

    /**
     * Creates a string for a stimulus controller action.
     *
     * @param string $controller
     * @param string $event
     * @param string $method
     *
     * @return string
     */
    public function renderStimulusActionFunction($controller, $event, $method): string
    {
        return sprintf('%s->%s#%s', $event, $controller, $method);
    }

    /**
     * Converts snake_cased strings to kebab-case.
     *
     * @param string $string
     *
     * @return string
     */
    public function kebabCaseFilter(string $string)
    {
        return $this->slugifier->slugify($string);
    }

    /**
     * Converts kebab-cased strings to snake_case.
     *
     * @param string $string
     *
     * @return string
     */
    public function snakeCaseFilter(string $string)
    {
        return $this->slugifier->slugify($string, '_');
    }
}

<?
require_once 'config.php';

$json = file_get_contents('http://localhost:32322/fppd/schedule');
$data = json_decode($json, true);
$schedulerEnabled = isset($data['schedule']['enabled']) ? $data['schedule']['enabled'] : 1;
?>
<style>
    /*
     * FullCalendar builds its own DOM and reads these custom properties for
     * every border, background and button it paints.  Mapping them onto the FPP
     * design tokens is what makes the calendar follow the light/dark theme
     * without restyling its internals.
     */
    #scheduleCalendarWrapper {
        --fc-page-bg-color: var(--fpp-bg-card);
        --fc-neutral-bg-color: var(--fpp-bg-hover);
        --fc-neutral-text-color: var(--fpp-text-secondary);
        --fc-border-color: var(--fpp-border);
        --fc-today-bg-color: var(--bs-primary-bg-subtle);
        --fc-now-indicator-color: var(--bs-danger);
        --fc-button-text-color: var(--bs-body-color);
        --fc-button-bg-color: transparent;
        --fc-button-border-color: var(--fpp-border);
        --fc-button-hover-bg-color: var(--fpp-bg-hover);
        --fc-button-hover-border-color: var(--fpp-border-dark);
        --fc-button-active-bg-color: var(--bs-primary);
        --fc-button-active-border-color: var(--bs-primary);
        --fc-event-border-color: transparent;
        /*
         * FullCalendar paints the event text from this property on an inner
         * element, so it beats any colour set on the event itself.  Its default
         * is white, which vanishes against the subtle backgrounds below in the
         * light theme; following the emphasis colour keeps it legible in both.
         */
        --fc-event-text-color: var(--bs-emphasis-color);
        color: var(--fpp-text-primary);
    }

    /*
     * Event colouring is driven by classes rather than per-event colours so the
     * palette stays with the theme.  Each schedule entry type gets its own
     * accent; overridden and disabled occurrences are muted so they read as
     * "will not run" at a glance.
     */
    .sch-cal-event {
        border-left: 4px solid var(--bs-primary);
        background-color: var(--bs-primary-bg-subtle);
        color: var(--bs-emphasis-color);
    }

    .sch-cal-event-command {
        border-left-color: var(--bs-info);
        background-color: var(--bs-info-bg-subtle);
    }

    .sch-cal-event-sequence {
        border-left-color: var(--bs-success);
        background-color: var(--bs-success-bg-subtle);
    }

    .sch-cal-event-overridden {
        border-left-color: var(--bs-danger);
        background-color: var(--bs-danger-bg-subtle);
        text-decoration: line-through;
    }

    .sch-cal-event-disabled {
        border-left-color: var(--fpp-border-dark);
        background-color: var(--fpp-bg-disabled);
        --fc-event-text-color: var(--fpp-text-muted);
        color: var(--fpp-text-muted);
        opacity: 0.75;
    }

    /* Legend swatches mirror the event accents above. */
    .sch-cal-swatch {
        display: inline-block;
        width: 0.85rem;
        height: 0.85rem;
        border-radius: 2px;
        vertical-align: text-bottom;
    }
</style>

<div id="scheduleCalendarWrapper">
    <?php if (!$schedulerEnabled) { ?>
        <div class="alert alert-warning py-2 mb-2">
            <i class="fas fa-exclamation-triangle"></i> The scheduler is currently
            <b>disabled</b>. Nothing below will run until it is re-enabled.
        </div>
    <?php } ?>

    <div class="d-flex flex-wrap align-items-center gap-3 mb-2">
        <div class="form-check form-switch mb-0">
            <input class="form-check-input" type="checkbox" id="schCalShowDisabled">
            <label class="form-check-label" for="schCalShowDisabled">Show disabled entries</label>
        </div>

        <div class="form-check form-switch mb-0">
            <input class="form-check-input" type="checkbox" id="schCalHideOverridden" checked>
            <label class="form-check-label" for="schCalHideOverridden">Hide overridden</label>
        </div>

        <div class="d-flex flex-wrap align-items-center gap-3 ms-auto small">
            <span><span class="sch-cal-swatch sch-cal-event"></span> Playlist</span>
            <span><span class="sch-cal-swatch sch-cal-event sch-cal-event-sequence"></span> Sequence</span>
            <span><span class="sch-cal-swatch sch-cal-event sch-cal-event-command"></span> Command</span>
            <span><span class="sch-cal-swatch sch-cal-event sch-cal-event-overridden"></span> Overridden</span>
            <span><span class="sch-cal-swatch sch-cal-event sch-cal-event-disabled"></span> Disabled</span>
        </div>
    </div>

    <div id="scheduleCalendar"></div>

    <p class="text-muted small mt-2 mb-0">
        Occurrences are expanded from the schedule rules, so any month can be
        previewed. FPPD itself only commits the next
        <b><?php echo GetSettingValue('ScheduleDistance', 28); ?></b> days to its
        run queue &mdash; dates beyond that are shown here as planned, not queued.
    </p>
</div>

-- FPP: Block default target fallback for combine-stream and filter-chain nodes
--
-- Problem: When WirePlumber cannot find the defined target for a node,
-- find-default-target links it to the default sink. This causes:
--   1. Filter-chain outputs link back to the combine sink, creating a
--      link-group loop that prevents combine-output -> filter-chain links.
--   2. Combine outputs get rogue links to the default ALSA sink (doubled audio).
--
-- Solution: Block the default-target fallback for FPP nodes that have an
-- explicit target set. On rescan (when the target appears), find-defined-target
-- will succeed and create the correct link.

lutils = require ("linking-utils")
log = Log.open_topic ("s-fpp-linking")

SimpleEventHook {
  name = "linking/fpp-block-combine-fallback",
  after = { "linking/find-defined-target", "linking/find-filter-target" },
  before = { "linking/find-default-target", "linking/find-best-target" },
  interests = {
    EventInterest {
      Constraint { "event.type", "=", "select-target" },
    },
  },
  execute = function (event)
    local source, om, si, si_props, si_flags, target =
        lutils:unwrap_select_target_event (event)

    -- If a target was already found, let it proceed normally
    if target then
      return
    end

    local node_name = si_props ["node.name"] or ""
    local has_target = si_props ["node.target"] ~= nil or
                       si_props ["target.object"] ~= nil

    -- Block default fallback for FPP output group combine-stream outputs
    if node_name:match ("^output%.fpp_group_") then
      log:info (si, "... FPP combine output: blocking default fallback for "
          .. node_name .. ", will retry on rescan")
      event:stop_processing ()
      return
    end

    -- Block default fallback for FPP input group combine-stream outputs
    if node_name:match ("^output%.fpp_input_") then
      log:info (si, "... FPP input group output: blocking default fallback for "
          .. node_name .. ", will retry on rescan")
      event:stop_processing ()
      return
    end

    -- Block default fallback for FPP routing hub combine-stream outputs (post-effects)
    if node_name:match ("^output%.fpp_route_ig_") then
      log:info (si, "... FPP routing hub output: blocking default fallback for "
          .. node_name .. ", will retry on rescan")
      event:stop_processing ()
      return
    end

    -- Block default fallback for FPP filter-chain outputs with explicit target
    -- (prevents linking back to combine sink when target isn't ready yet)
    if node_name:match ("^fpp_fx_g%d+_.*_out$") and has_target then
      log:info (si, "... FPP filter-chain output: blocking default fallback for "
          .. node_name .. ", target: " .. tostring (si_props ["node.target"])
          .. ", will retry on rescan")
      event:stop_processing ()
      return
    end

    -- Block default fallback for FPP input group filter-chain outputs with explicit target
    if node_name:match ("^fpp_fx_ig_%d+_out$") and has_target then
      log:info (si, "... FPP input group EQ output: blocking default fallback for "
          .. node_name .. ", target: " .. tostring (si_props ["node.target"])
          .. ", will retry on rescan")
      event:stop_processing ()
      return
    end

    -- Block default fallback for FPP input group loopback nodes with explicit target
    -- (matches both bare and sub-node names: fpp_loopback_ig1_*, input.fpp_loopback_ig1_*, output.fpp_loopback_ig1_*)
    if (node_name:match ("^fpp_loopback_ig%d+_") or node_name:match ("^[io][nu]%a+%.fpp_loopback_ig%d+_")) and has_target then
      log:info (si, "... FPP input loopback: blocking default fallback for "
          .. node_name .. ", will retry on rescan")
      event:stop_processing ()
      return
    end

    -- Block default fallback for FPP tee (null-sink fan-out) nodes
    if node_name:match ("^fpp_tee_fppd_stream_") then
      log:info (si, "... FPP tee node: blocking default fallback for "
          .. node_name .. ", will retry on rescan")
      event:stop_processing ()
      return
    end

    -- Block default fallback for fppd stream nodes with explicit target
    -- (prevents routing to wrong default sink when input group isn't ready)
    if node_name:match ("^fppd_stream_%d+") and has_target then
      log:info (si, "... FPP media stream: blocking default fallback for "
          .. node_name .. ", will retry on rescan")
      event:stop_processing ()
      return
    end

    -- Block default fallback for FPP video stream nodes with explicit target
    -- (video routed through PipeWire to video output groups)
    if node_name:match ("^fppd_video_stream_%d+") and has_target then
      log:info (si, "... FPP video stream: blocking default fallback for "
          .. node_name .. ", will retry on rescan")
      event:stop_processing ()
      return
    end

    -- Block default fallback for FPP video output consumer nodes with explicit target
    if node_name:match ("^fpp_video_out_") and has_target then
      log:info (si, "... FPP video output: blocking default fallback for "
          .. node_name .. ", will retry on rescan")
      event:stop_processing ()
      return
    end

    -- Block default fallback for FPP video group combine-stream outputs
    if node_name:match ("^output%.fpp_video_group_") then
      log:info (si, "... FPP video group output: blocking default fallback for "
          .. node_name .. ", will retry on rescan")
      event:stop_processing ()
      return
    end

    -- (Routing loopback nodes removed — combine-stream handles output routing)
  end
}:register ()

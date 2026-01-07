# HTTPVirtualDisplay3D - True 3D Virtual Display Support

## Overview
This adds a new 3D-specific virtual display backend endpoint that sends pixel data by index instead of 2D coordinates, enabling true 3D effects where each pixel is independent.

## Files Created
- `/opt/fpp/src/channeloutput/HTTPVirtualDisplay3D.h` - Header file
- `/opt/fpp/src/channeloutput/HTTPVirtualDisplay3D.cpp` - Implementation
- `/opt/fpp/src/makefiles/libfpp-co-HTTPVirtualDisplay3D.mk` - Build configuration

## Files Modified
- `/opt/fpp/etc/apache2.site` - Added proxy rule for `/api/http-virtual-display-3d/`
- `/opt/fpp/www/virtualdisplaybody3d.php` - Updated to use new endpoint and pixel index lookup
- `/opt/fpp/www/co-other-modules.php` - Added HTTPVirtualDisplay3D class and registration

## Build and Installation

### 1. Build the Plugin

Navigate to the FPP source directory and build:
```bash
cd /opt/fpp/src
make libfpp-co-HTTPVirtualDisplay3D.so
```

The `.so` file will be created in `/opt/fpp/src/` where fppd loads it from at runtime. No manual installation needed.

### 2. Update Apache Configuration

The Apache proxy configuration needs to be activated:
```bash
sudo cat /opt/fpp/etc/apache2.site > /etc/apache2/sites-enabled/000-default.conf
sudo systemctl restart apache2
```

### 3. Restart FPPD

```bash
sudo systemctl restart fppd
```

Check that the plugin loaded:
```bash
sudo netstat -tulpn | grep 32329
```

You should see fppd listening on port 32329.

## Configuration

### Via FPP Web UI (Recommended)

1. Go to Status/Control → Channel Outputs → Other tab
2. Click "Add" to add a new output
3. Select "HTTP Virtual Display 3D" from the Type dropdown
4. Configure settings:
   - **Enabled**: Yes
   - **Start Channel**: 1 (or your starting channel)
   - **Channel Count**: Total number of RGB channels (pixels × 3)
   - **Port**: 32329 (default)
   - **Update Interval**: 1ms (default)
5. Click "Set Channel Outputs"

### Manual Configuration (co-other.json)

If needed, you can manually edit `/home/fpp/media/config/co-other.json`:
```json
{
    "type": "HTTPVirtualDisplay3D",
    "enabled": 1,
    "startChannel": 1,
    "channelCount": 547683,
    "port": 32329,
    "updateInterval": 1
}
```

## How It Works

### Original HTTPVirtualDisplay (2D):
- Groups pixels by 2D screen coordinates (x, y)
- All pixels at same X,Y position get the same color
- Data format: `RGB:xy_coord;xy_coord|RGB:xy_coord`
- Port: 32328
- Endpoint: `/api/http-virtual-display/`

### New HTTPVirtualDisplay3D:
- Each pixel is independent (uses array index)
- Pixels at same X,Y but different Z get different colors
- Data format: `RGB:index;index;index|RGB:index`
- Port: 32329
- Endpoint: `/api/http-virtual-display-3d/`

## Data Format Example

### 2D Format (old):
```
+Rv:AB12;CD34|+Sv:AB12
```
Means: Color +Rv for pixels at coords AB12 and CD34, Color +Sv for pixel at AB12

### 3D Format (new):
```
+Rv:0;1;2|+Sv:3;4
```
Means: Color +Rv for pixels 0, 1, 2, Color +Sv for pixels 3, 4

## Testing

1. Reload the 3D visualizer page
2. Console should show: `Starting SSE connection to api/http-virtual-display-3d/`
3. Run a sequence with 3D effects
4. Each pixel should now display independently

## Troubleshooting

### Connection refused on port 32329
- Check if FPPD loaded the plugin: `sudo tail -f /var/log/fpp/fppd.log`
- Should see: `HTTPVirtualDisplay3D listening on port 32329`

### Still seeing grouped pixels
- Check browser console - should connect to `/api/http-virtual-display-3d/`
- Hard refresh the page (Ctrl+Shift+R)

### Compilation errors
- Ensure you have the full FPP development environment
- Check that `VirtualDisplayBase.h` exists in channeloutput/

## Reverting to 2D Mode

To switch back to 2D mode, change line 1950 in `/opt/fpp/www/virtualdisplaybody3d.php`:
```javascript
evtSource = new EventSource('api/http-virtual-display/');  // 2D mode
// evtSource = new EventSource('api/http-virtual-display-3d/');  // 3D mode
```

## Performance Notes

The 3D endpoint sends more distinct pixel updates but uses more compact pixel indices instead of coordinate strings, so bandwidth should be similar for most scenarios.

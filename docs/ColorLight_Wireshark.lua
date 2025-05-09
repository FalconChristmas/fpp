-- ColorLight 5A 75B Protocol Dissector
-- Save this script as colorlight.lua

local ColorLightProtocol = Proto("ColorLight", "ColorLight 5A 75B Protocol")

local f = ColorLightProtocol.fields

-- Configure Panel Layout (Type 0x02)
f.rcvr_width = ProtoField.uint16("colorlight.rcvr_width", "Receiver Width", base.DEC)
f.rcvr_height = ProtoField.uint16("colorlight.rcvr_height", "Receiver Height", base.DEC)
f.next_rcvr_xoffset = ProtoField.uint16("colorlight.next_rcvr_xoffset", "Next Receiver xOffset", base.DEC)
f.next_rcvr_yoffset = ProtoField.uint16("colorlight.next_rcvr_yoffset", "Next Receiver yOffset", base.DEC)
f.total_width = ProtoField.uint16("colorlight.total_width", "Total Width", base.DEC)
f.total_height = ProtoField.uint16("colorlight.total_height", "Total Height", base.DEC)

-- Receiver Response (Type 0x08)
f.firmware_major = ProtoField.uint8("colorlight.firmware_major", "Firmware Major", base.DEC)
f.firmware_minor = ProtoField.uint8("colorlight.firmware_minor", "Firmware Minor", base.DEC)
f.uptime = ProtoField.uint32("colorlight.uptime", "Uptime (ms)", base.DEC)
f.packet_count = ProtoField.uint32("colorlight.packet_count", "Packet Count", base.DEC)

-- Pixel Data Frame (Type 0x55)
f.pixel_row_number = ProtoField.uint16("colorlight.pixel_row_number", "Row Number", base.DEC)
f.pixel_offset = ProtoField.uint16("colorlight.pixel_offset", "Pixel Offset", base.DEC)
f.pixel_count = ProtoField.uint16("colorlight.pixel_count", "Pixel Count", base.DEC)
-- does it need to be LE?
--f.pixel_count = ProtoField.uint16("colorlight.pixel_count", "Pixel Count", base.DEC, nil, nil, "Little-endian")
f.pixel_data = ProtoField.bytes("colorlight.pixel_data", "Pixel Data")

-- Used by multiple packet types
f.src_mac = ProtoField.string("colorlight.src_mac", "Source", base.ASCII)
f.dst_mac = ProtoField.string("colorlight.dst_mac", "Destination", base.ASCII)
f.packet_type = ProtoField.uint8("colorlight.type", "Packet Type", base.HEX)
f.receiver = ProtoField.uint8("colorlight.receiver", "Receiver ID", base.DEC)
f.brightness = ProtoField.uint8("colorlight.brightness", "Brightness", base.DEC)
f.brightnessB = ProtoField.uint8("colorlight.brightnessB", "Brightness-B", base.DEC)
f.brightnessG = ProtoField.uint8("colorlight.brightnessG", "Brightness-G", base.DEC)
f.brightnessR = ProtoField.uint8("colorlight.brightnessR", "Brightness-R", base.DEC)
f.unknown00 = ProtoField.uint8("colorlight.unknown00", "Unknown-00", base.HEX)
f.unknown01 = ProtoField.uint8("colorlight.unknown01", "Unknown-01", base.HEX)
f.unknown02 = ProtoField.uint8("colorlight.unknown02", "Unknown-02", base.HEX)
f.unknown03 = ProtoField.uint8("colorlight.unknown03", "Unknown-03", base.HEX)
f.unknown04 = ProtoField.uint8("colorlight.unknown04", "Unknown-04", base.HEX)
f.unknown05 = ProtoField.uint8("colorlight.unknown05", "Unknown-05", base.HEX)
f.unknown06 = ProtoField.uint8("colorlight.unknown06", "Unknown-06", base.HEX)
f.unknown07 = ProtoField.uint8("colorlight.unknown07", "Unknown-07", base.HEX)
f.unknown08 = ProtoField.uint8("colorlight.unknown08", "Unknown-08", base.HEX)
f.unknown09 = ProtoField.uint8("colorlight.unknown09", "Unknown-09", base.HEX)

-- Dissector function
function ColorLightProtocol.dissector(buffer, pinfo, tree)
    -- registered in 'ethertype', buffer starts after the EtherType field
    local data_length = buffer:len()

    -- Check that the packet is large enough
    if data_length < 13 then return end

    -- Check that the packet contains the ColorLight MAC as sender or receiver
    if tostring(buffer(0,6):ether()) ~= "11:22:33:44:55:66" and tostring(buffer(6,6):ether()) ~= "11:22:33:44:55:66" then return end

    -- Add protocol to tree
    local subtree = tree:add(ColorLightProtocol, buffer(), "ColorLight Protocol Data")
    subtree:add(f.dst_mac, buffer(0, 6), tostring(buffer(0,6):ether()))
    subtree:add(f.src_mac, buffer(6, 6), tostring(buffer(6,6):ether()))

    pinfo.cols.src = tostring(buffer(6,6):ether())
    pinfo.cols.dst = tostring(buffer(0,6):ether())
    pinfo.cols.protocol = "ColorLight"

    local data_offset = 13

    local packet_type = buffer(12, 1):uint()
    subtree:add(f.packet_type, buffer(12, 1), packet_type)

    -- Handle other frame types if needed
    pinfo.cols.info = string.format("Unknown, Possibly not ColorLight, packet type: 0x%02X", packet_type)

    -- Sync (Display Frame) Packet
    if packet_type == 0x01 then
        pinfo.cols.info = "Sync Display"

        local brightness = buffer(data_offset + 22, 1):uint()
        subtree:add(f.brightness, buffer(data_offset + 22, 1), brightness)

        local brightnessB = buffer(data_offset + 25, 1):uint()
        subtree:add(f.brightnessB, buffer(data_offset + 25, 1), brightnessB)

        local brightnessG = buffer(data_offset + 26, 1):uint()
        subtree:add(f.brightnessG, buffer(data_offset + 26, 1), brightnessG)

        local brightnessR = buffer(data_offset + 27, 1):uint()
        subtree:add(f.brightnessR, buffer(data_offset + 27, 1), brightnessR)

        return 1
    end

    -- Configure Receiver Layout
    if packet_type == 0x02 then
	pinfo.cols.info = "Configure Receiver Layout"

    	local rcvr_tree = subtree:add(ColorLightProtocol, buffer(data_offset + 3, 20 * 64), "Receivers")

	local id = 0
	local rcvr_offset = 3
	for r=0,63 do
    	    local rcvr_br = rcvr_tree:add(ColorLightProtocol, buffer(data_offset + 3 + (r * 20), 20),
    		string.format("Receiver ID: %d", id))

            local rcvr_width = buffer(data_offset + rcvr_offset + 4, 2):uint()
            rcvr_br:add(f.rcvr_width, buffer(data_offset + rcvr_offset + 4, 2), rcvr_width)

            local rcvr_height = buffer(data_offset + rcvr_offset + 6, 2):uint()
            rcvr_br:add(f.rcvr_height, buffer(data_offset + rcvr_offset + 6, 2), rcvr_height)

            local next_rcvr_xoffset = buffer(data_offset + rcvr_offset + 10, 2):uint()
            rcvr_br:add(f.next_rcvr_xoffset, buffer(data_offset + rcvr_offset + 10, 2), next_rcvr_xoffset)

            local next_rcvr_yoffset = buffer(data_offset + rcvr_offset + 12, 2):uint()
            rcvr_br:add(f.next_rcvr_yoffset, buffer(data_offset + rcvr_offset + 12, 2), next_rcvr_yoffset)

            local total_width = buffer(data_offset + rcvr_offset + 14, 2):uint()
            rcvr_br:add(f.total_width, buffer(data_offset + rcvr_offset + 14, 2), total_width)

            local total_height = buffer(data_offset + rcvr_offset + 16, 2):uint()
            rcvr_br:add(f.total_height, buffer(data_offset + rcvr_offset + 16, 2), total_height)

	    id = id + 1
	    rcvr_offset = rcvr_offset + 20
	end

	return 1
    end

    -- Discovery Packet
    if packet_type == 0x07 then
        local receiverID = buffer(data_offset + 3, 1):uint()
        subtree:add(f.receiver, buffer(data_offset + 3, 1), receiverID)

        pinfo.cols.info = string.format("Receiver Query, ReceiverID: %d", receiverID)

        return 1
    end

    -- Discovery Response
    if packet_type == 0x08 then
        local receiverID = buffer(data_offset + 85, 1):uint()
        subtree:add(f.receiver, buffer(data_offset + 85, 1), receiverID)

        local firmware_major = buffer(data_offset + 2, 1):uint()
        subtree:add(f.firmware_major, buffer(data_offset + 2, 1), firmware_major)
        local firmware_minor = buffer(data_offset + 3, 1):uint()
        subtree:add(f.firmware_minor, buffer(data_offset + 3, 1), firmware_minor)

        local uptime = buffer(data_offset + 46, 4):uint()
        subtree:add(f.uptime, buffer(data_offset + 46, 4), uptime)

        local packet_count = buffer(data_offset + 38, 4):uint()
        subtree:add(f.packet_count, buffer(data_offset + 38, 4), packet_count)

        pinfo.cols.info = string.format("Receiver Response, ReceiverID: %d", receiverID)

        return 1
    end

    -- Save Config??
    if packet_type == 0x11 then

        local receiverID = buffer(data_offset + 3, 1):uint()
        subtree:add(f.receiver, buffer(data_offset + 3, 1), receiverID)

        pinfo.cols.info = string.format("Save Config?? ReceiverID: %d", receiverID)

	return 1
    end

    -- Brightness Packet
    if packet_type == 0x0A then
        pinfo.cols.info = "Brightness"

        local brightnessB = buffer(data_offset + 0, 1):uint()
        subtree:add(f.brightnessB, buffer(data_offset + 0, 1), brightnessB)

        local brightnessG = buffer(data_offset + 1, 1):uint()
        subtree:add(f.brightnessG, buffer(data_offset + 1, 1), brightnessG)

        local brightnessR = buffer(data_offset + 2, 1):uint()
        subtree:add(f.brightnessR, buffer(data_offset + 2, 1), brightnessR)

        return 1
    end

    -- Unknown Packets - sent when sending config to receiver
    if packet_type == 0x10 or packet_type == 0x18 or packet_type == 0x1F or packet_type == 0x26 or packet_type == 0x31 or packet_type == 0x32 or packet_type == 0x76 then
        pinfo.cols.info = string.format("Unparsed ColorLight Packet. Type: 0x%02X", packet_type)

        return 1
    end

    -- Pixel Data Packet
    if packet_type == 0x55 then
        -- Row Number
        local row_number = buffer(data_offset, 2):uint()
        subtree:add(f.pixel_row_number, buffer(data_offset, 2), row_number)

        pinfo.cols.info = "Pixel Data Frame, Row: " .. tostring(row_number)

        -- Pixel Offset (little-endian)
        local pixel_offset = buffer(data_offset + 2, 2):uint()
        subtree:add(f.pixel_offset, buffer(data_offset + 2, 2), pixel_offset)

        -- Pixel Count (little-endian)
        local pixel_count = buffer(data_offset + 4, 2):uint()
        subtree:add(f.pixel_count, buffer(data_offset + 4, 2), pixel_count)

        -- Pixel Data
        local pixel_data_length = buffer:len() - 21
        if pixel_data_length > 0 then
            subtree:add(f.pixel_data, buffer(21, pixel_data_length))
        end

        return 1
    end

    return 0
end

-- Register the dissector for specific EtherTypes
local eth_table = DissectorTable.get("wtap_encap")
eth_table:add(1, ColorLightProtocol)

-- Register as a post-dissector so non-ColorLight packets are also decoded
-- register_postdissector(ColorLightProtocol)





class_name DaxSpriteLoader
extends RefCounted

const EGA_PALETTE := [
	Color8(0, 0, 0, 0), Color8(0, 0, 170), Color8(0, 170, 0), Color8(0, 170, 170),
	Color8(170, 0, 0), Color8(170, 0, 170), Color8(170, 85, 0), Color8(170, 170, 170),
	Color8(85, 85, 85), Color8(85, 85, 255), Color8(85, 255, 85), Color8(85, 255, 255),
	Color8(255, 85, 85), Color8(0, 0, 0), Color8(255, 255, 85), Color8(255, 255, 255)
]

var error_message := ""

func list_record_ids(path: String) -> Array[int]:
	error_message = ""
	var file := FileAccess.open(path, FileAccess.READ)
	if file == null:
		error_message = "Could not open %s" % path
		return []
	var dax := file.get_buffer(file.get_length())
	if dax.size() < 2:
		error_message = "Truncated DAX archive"
		return []
	var toc_size := _u16(dax, 0)
	if toc_size % 9 != 0 or 2 + toc_size > dax.size():
		error_message = "Invalid DAX table of contents"
		return []
	var ids: Array[int] = []
	for header in range(2, 2 + toc_size, 9):
		var record_id: int = dax[header]
		if not ids.has(record_id):
			ids.append(record_id)
	ids.sort()
	if ids.is_empty():
		error_message = "DAX archive contains no records"
	return ids

func frame_count(path: String, record_id: int, combat: bool = false) -> int:
	error_message = ""
	var file := FileAccess.open(path, FileAccess.READ)
	if file == null:
		error_message = "Could not open %s" % path
		return 0
	var record := _extract_record(file.get_buffer(file.get_length()), record_id)
	if record.is_empty():
		return 0
	if combat:
		return _combat_frame_count(record)
	return record[0]

func _combat_frame_count(record: PackedByteArray) -> int:
	if record.size() < 17:
		error_message = "Truncated combat image header"
		return 0
	var height := _u16(record, 0)
	var width_bytes := _u16(record, 2)
	var count: int = record[8]
	if height == 0 or height > 200 or width_bytes == 0 or width_bytes > 40 or count == 0 or record.size() != 17 + height * width_bytes * 4 * count:
		error_message = "Invalid combat image dimensions or payload size"
		return 0
	return count

func load_frame(path: String, record_id: int, frame_index: int = 0, combat: bool = false) -> Image:
	error_message = ""
	var file := FileAccess.open(path, FileAccess.READ)
	if file == null:
		error_message = "Could not open %s" % path
		return null
	var dax := file.get_buffer(file.get_length())
	var record := _extract_record(dax, record_id)
	if record.is_empty():
		return null
	if combat:
		var count := _combat_frame_count(record)
		if count == 0:
			return null
		if frame_index < 0 or frame_index >= count:
			error_message = "Combat image index out of range"
			return null
		var height := _u16(record, 0)
		var width_bytes := _u16(record, 2)
		return _decode_pixels(record, 17 + frame_index * height * width_bytes * 4, width_bytes * 8, height, true)
	if frame_index < 0 or frame_index >= record[0]:
		error_message = "Frame %d is not present in record %d" % [frame_index, record_id]
		return null
	var cursor := 1
	for frame in range(record[0]):
		if cursor + 21 > record.size():
			error_message = "Truncated EGA sprite header"
			return null
		var height := _u16(record, cursor + 4)
		var width_bytes := _u16(record, cursor + 6)
		var packed_size := height * width_bytes * 4
		var pixel_start := cursor + 21
		if height == 0 or width_bytes == 0 or pixel_start + packed_size > record.size():
			error_message = "Invalid EGA sprite dimensions"
			return null
		if frame == frame_index:
			return _decode_pixels(record, pixel_start, width_bytes * 8, height)
		cursor = pixel_start + packed_size
	error_message = "Sprite frame was not found"
	return null

func _extract_record(dax: PackedByteArray, record_id: int) -> PackedByteArray:
	if dax.size() < 2:
		error_message = "Truncated DAX archive"
		return PackedByteArray()
	var toc_size := _u16(dax, 0)
	if toc_size % 9 != 0 or 2 + toc_size > dax.size():
		error_message = "Invalid DAX table of contents"
		return PackedByteArray()
	for entry in range(int(toc_size / 9)):
		var header := 2 + entry * 9
		if dax[header] != record_id:
			continue
		var offset := _u32(dax, header + 1)
		var raw_size := _u16(dax, header + 5)
		var compressed_size := _u16(dax, header + 7)
		var start := 2 + toc_size + offset
		if start + compressed_size > dax.size():
			error_message = "Truncated DAX record"
			return PackedByteArray()
		return _decompress(dax.slice(start, start + compressed_size), raw_size)
	error_message = "Record %d was not found" % record_id
	return PackedByteArray()

func _decompress(input: PackedByteArray, raw_size: int) -> PackedByteArray:
	var output := PackedByteArray()
	var cursor := 0
	while cursor < input.size():
		var command: int = input[cursor]
		cursor += 1
		if command >= 128:
			command -= 256
		if command >= 0:
			var count := command + 1
			if cursor + count > input.size():
				error_message = "Truncated DAX literal run"
				return PackedByteArray()
			output.append_array(input.slice(cursor, cursor + count))
			cursor += count
		else:
			if cursor >= input.size():
				error_message = "Truncated DAX repeat run"
				return PackedByteArray()
			for ignored in range(-command):
				output.append(input[cursor])
			cursor += 1
	if output.size() != raw_size:
		error_message = "DAX record expanded to %d bytes; expected %d" % [output.size(), raw_size]
		return PackedByteArray()
	return output

func _decode_pixels(data: PackedByteArray, start: int, width: int, height: int, combat: bool = false) -> Image:
	var rgba := PackedByteArray()
	rgba.resize(width * height * 4)
	for pixel in range(width * height):
		var packed: int = data[start + (pixel >> 1)]
		var index: int = packed >> 4 if pixel % 2 == 0 else packed & 0x0f
		var color: Color = EGA_PALETTE[index]
		if combat and index == 8:
			color = Color8(0, 0, 0)
		elif combat and index == 13:
			color = Color8(255, 85, 255)
		var out := pixel * 4
		rgba[out] = color.r8
		rgba[out + 1] = color.g8
		rgba[out + 2] = color.b8
		rgba[out + 3] = 0 if index == 0 else 255
	return Image.create_from_data(width, height, false, Image.FORMAT_RGBA8, rgba)

func _u16(data: PackedByteArray, offset: int) -> int:
	return data[offset] | (data[offset + 1] << 8)

func _u32(data: PackedByteArray, offset: int) -> int:
	return data[offset] | (data[offset + 1] << 8) | (data[offset + 2] << 16) | (data[offset + 3] << 24)

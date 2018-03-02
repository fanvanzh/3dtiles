extern "C" {
    fn shp23dtile(name: *const u8, layer: i32, dest: *const u8) -> bool;
}

pub fn shape_batch_convert(from: &str, to: &str) {
    unsafe {
        let mut source_vec = from.as_bytes().to_vec();
        source_vec.push(0x00);
        let mut dest_vec = to.as_bytes().to_vec();
        dest_vec.push(0x00);
        shp23dtile(source_vec.as_ptr(), 0, dest_vec.as_ptr());
    }
}

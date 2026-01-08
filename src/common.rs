pub fn str_to_vec_c(str: &str) -> Vec<u8> {
    let mut buf = str.as_bytes().to_vec();
    buf.push(0x00);
    buf
}
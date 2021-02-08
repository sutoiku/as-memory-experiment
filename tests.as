export function testRead(address: usize): u8 {
  return load<u8>(address);
}

export function testWrite(address: usize, value: u8): void {
  store<u8>(address, value);
}

func sum_loop(limit) {
  let sum = 0;
  let i = 0;
  while (i < limit) {
    if (i == 5) { i = i + 1; continue; }
    if (i == 8) { break; }
    sum = sum + i;
    i = i + 1;
  }
  return sum;
}

print_int(sum_loop(10));  // erwartet: 23

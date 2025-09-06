func sum_loop(limit) {
  let sum = 0;
  for (let i = 0; i < limit; i = i + 1) {
    if (i == 5) {
      continue;
    }
    if (i == 8) {
      break;
    }
    sum = sum + i;
  }
  return sum;
}

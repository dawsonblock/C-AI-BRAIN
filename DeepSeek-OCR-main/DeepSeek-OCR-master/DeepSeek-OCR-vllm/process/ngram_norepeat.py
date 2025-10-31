import torch
from transformers import LogitsProcessor
from transformers.generation.logits_process import _calc_banned_ngram_tokens
from typing import List, Set


class NoRepeatNGramLogitsProcessor(LogitsProcessor):

    def __init__(self, ngram_size: int, window_size: int = 100, whitelist_token_ids: set = None):
        if not isinstance(ngram_size, int) or ngram_size <= 0:
            raise ValueError(f"`ngram_size` has to be a strictly positive integer, but is {ngram_size}")
        if not isinstance(window_size, int) or window_size <= 0:
            raise ValueError(f"`window_size` has to be a strictly positive integer, but is {window_size}")
        self.ngram_size = ngram_size
        self.window_size = window_size
        self.whitelist_token_ids = whitelist_token_ids or set()
    
    def __call__(self, input_ids: List[int], scores: torch.FloatTensor) -> torch.FloatTensor:
        num_input_ids = len(input_ids)
        if num_input_ids < self.ngram_size:
            return scores

        search_start = max(0, num_input_ids - self.window_size)

        ngrams = {tuple(input_ids[i:i + self.ngram_size]) for i in range(search_start, num_input_ids - self.ngram_size + 1)}
        current_prefix = tuple(input_ids[-(self.ngram_size - 1):])

        banned_tokens = set()
        for ngram in ngrams:
            if ngram[:-1] == current_prefix:
                banned_tokens.add(ngram[-1])

        banned_tokens -= self.whitelist_token_ids

        if banned_tokens:
            vocab_size = scores.size(-1)
            # Filter to valid index range to avoid IndexError
            valid_banned = [t for t in banned_tokens if 0 <= t < vocab_size]
            if valid_banned:
                scores = scores.clone()
                scores[valid_banned] = -float("inf")

        return scores
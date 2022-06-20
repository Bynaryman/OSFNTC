#!/usr/bin/env python
import ctranslate2
import sentencepiece as spm
import pathlib
file_dir = pathlib.Path(__file__).parent.resolve()

translator = ctranslate2.Translator(str(file_dir)+"/ende_ctranslate2/", device="cpu")
sp = spm.SentencePieceProcessor(str(file_dir)+"/sentencepiece.model")

input_text = "Hello world!"
input_tokens = sp.encode(input_text, out_type=str)

results = translator.translate_batch([input_tokens])

output_tokens = results[0].hypotheses[0]
output_text = sp.decode(output_tokens)

print(output_text)

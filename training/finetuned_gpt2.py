import argparse
import os
import random
from pathlib import Path

import torch
from datasets import Dataset
from transformers import (
    AutoModelForCausalLM,
    AutoTokenizer,
    DataCollatorForLanguageModeling,
    Trainer,
    TrainingArguments,
    set_seed,
)


def read_text_lines(path: str):
    with open(path, "r", encoding="utf-8") as f:
        lines = [line.strip() for line in f.readlines()]
    return [line for line in lines if line]


def build_dataset(lines, block_prefix=""):
    texts = []
    for line in lines:
        if block_prefix:
            texts.append(f"{block_prefix}{line}")
        else:
            texts.append(line)
    return Dataset.from_dict({"text": texts})


def split_dataset(dataset, eval_ratio=0.1, seed=42):
    total = len(dataset)
    eval_size = max(1, int(total * eval_ratio))
    train_size = total - eval_size
    shuffled = dataset.shuffle(seed=seed)
    train_dataset = shuffled.select(range(train_size))
    eval_dataset = shuffled.select(range(train_size, total))
    return train_dataset, eval_dataset


def tokenize_function(examples, tokenizer, max_length):
    return tokenizer(
        examples["text"],
        truncation=True,
        max_length=max_length,
        padding="max_length",
    )


def generate_sample(model, tokenizer, prompt, device, max_new_tokens=80):
    inputs = tokenizer(prompt, return_tensors="pt").to(device)
    with torch.no_grad():
        outputs = model.generate(
            **inputs,
            max_new_tokens=max_new_tokens,
            do_sample=True,
            temperature=0.8,
            top_k=50,
            top_p=0.95,
            pad_token_id=tokenizer.eos_token_id,
        )
    return tokenizer.decode(outputs[0], skip_special_tokens=True)


def main():
    parser = argparse.ArgumentParser(description="Fine-tune GPT-2 style model for AI Dungeon narrative.")
    parser.add_argument("--data", type=str, default="training/data/adventure_lines.txt")
    parser.add_argument("--model", type=str, default="distilgpt2")
    parser.add_argument("--output", type=str, default="training/output/gpt2-dungeon")
    parser.add_argument("--epochs", type=int, default=3)
    parser.add_argument("--batch-size", type=int, default=2)
    parser.add_argument("--lr", type=float, default=5e-5)
    parser.add_argument("--max-length", type=int, default=128)
    parser.add_argument("--seed", type=int, default=42)
    args = parser.parse_args()

    set_seed(args.seed)
    random.seed(args.seed)

    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)

    if not os.path.exists(args.data):
        raise FileNotFoundError(f"Dataset file not found: {args.data}")

    lines = read_text_lines(args.data)
    if len(lines) < 10:
        raise ValueError("Dataset is too small. Add more narrative/action lines before training.")

    dataset = build_dataset(lines)
    train_dataset, eval_dataset = split_dataset(dataset, eval_ratio=0.1, seed=args.seed)

    tokenizer = AutoTokenizer.from_pretrained(args.model)
    if tokenizer.pad_token is None:
        tokenizer.pad_token = tokenizer.eos_token

    model = AutoModelForCausalLM.from_pretrained(args.model)
    model.config.pad_token_id = tokenizer.pad_token_id

    train_dataset = train_dataset.map(
        lambda x: tokenize_function(x, tokenizer, args.max_length),
        batched=True,
        remove_columns=["text"],
    )
    eval_dataset = eval_dataset.map(
        lambda x: tokenize_function(x, tokenizer, args.max_length),
        batched=True,
        remove_columns=["text"],
    )

    data_collator = DataCollatorForLanguageModeling(
        tokenizer=tokenizer,
        mlm=False,
    )

    training_args = TrainingArguments(
        output_dir=str(output_dir),
        num_train_epochs=args.epochs,
        per_device_train_batch_size=args.batch_size,
        per_device_eval_batch_size=args.batch_size,
        eval_strategy="epoch",
        save_strategy="epoch",
        logging_strategy="steps",
        logging_steps=10,
        learning_rate=args.lr,
        weight_decay=0.01,
        warmup_ratio=0.05,
        report_to="none",
        save_total_limit=2,
        load_best_model_at_end=False,
        fp16=torch.cuda.is_available(),
    )

    trainer = Trainer(
        model=model,
        args=training_args,
        train_dataset=train_dataset,
        eval_dataset=eval_dataset,
        data_collator=data_collator,
    )

    trainer.train()

    final_dir = output_dir / "final"
    final_dir.mkdir(parents=True, exist_ok=True)

    model.save_pretrained(final_dir)
    tokenizer.save_pretrained(final_dir)

    device = "cuda" if torch.cuda.is_available() else "cpu"
    model.to(device)
    prompt = "Player 1 enters a ruined forest clearing and notices a locked portal."
    sample = generate_sample(model, tokenizer, prompt, device)

    with open(output_dir / "sample_generation.txt", "w", encoding="utf-8") as f:
        f.write(sample)

    print(f"Training complete.")
    print(f"Saved model to: {final_dir}")
    print(f"Sample generation saved to: {output_dir / 'sample_generation.txt'}")


if __name__ == "__main__":
    main()
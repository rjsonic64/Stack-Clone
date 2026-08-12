# StackOverflow TSV Parser

A high-performance C++ application designed to import large TSV datasets into MySQL efficiently. The parser validates and cleans malformed records, batches SQL inserts, and supports additional SSH tunneling
features for remote database imports.

Designed to process datasets containing tens of millions of records effieciently.

## Features

- Parses large TSV datasets
- Cleans malformed rows and handles NULL values
- Multi-stage field validation
- JSON configuration for schemas, database, ssh
- Optimized Batch SQL insertion pipeline
- Supports datasets up to 13 GB
- Real time progress report with ETA
- Optional SSH tunneling using dedicated worker thread

-- 

## Technologies

- C++17
- libssh
- MySql
- MySQL X DevAPI (mysqlx)
- nlohmann/json
- std::thread

-- 

## How it works

1. Reads a TSV file from given directory.
2. Processes each line- separates fields, removes garbage, and validates each field by type and count.
3. Lines are loaded into chunks defined as a vector of strings and sent into the batch insertion function.
4. Chunks are processed line by line converting strings into valid SQL-safe values.
5. Batch rows into INSERT statements.
6. Import query into MySql via mysqlxAPI.
7. Displays progress and statistics.
8. Depending on SSH ON/OFF will run a TCP connection to remote machine via a separate thread.

-- 

## build

```bash
git clone ...
mkdir build
cd build
cmake ..
make
```

-- 

## Markdown

/Backend
	/Database
		database.cpp
		database.h
	/Parser
		/Config
			ParserConfig.json
		/Error_Logs
			/...
		parser.cpp
	/Schemas
		/...
	/SSH
		SSHClient.cpp
		SSHClient.h
	/Utils
		Exceptions.h

-- 

## Configuration

- Optional schema upload
- Database configurations to set DB name, URL for mysqlx, and remote port/host for SSH
- SSH configurations as well as ON/OFF
- Local machine configurations for SSH
- Path configurations for target file and schema directories

-- 

## Performance

- Tested with TSV files up to 13 GB
- Successfully processed over 28 million rows in approximately 24 minutes
- Batch inserts

-- 

## Challenges

- Designing clean architecture that was coherent and made sense optimization wise. Especially mid-way through trying to figure out which pieces did what.
For example- when writing the bulk load and cleaner functions I had to re-write them multiple times because the job I needed them to do shifted overtime
when system bottlenecks became more apparent.
- When creating the ssh pipeline problems arose when trying to do local port forwarding over libssh so I decided on manually building the socket myself
and dealing with byte transfer.
- Writing a fast, memory efficient, SQL-safe batching method.
- Discovering edge-cases from obvious to completely obscure.
- Handling 64-bit integer values

-- 

## Future Improvements

- Corrupted batch recovery
- Architecture tweaks(instead of cleaning and building INSERT statements separately do everything within one function and send to bulk load function)

-- 

## License

MIT

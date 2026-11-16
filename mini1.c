#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>

/*
For the language grammar, please refer to Grammar section on the github page:
  https://github.com/lightbulb12294/CSI2P-II-Mini#grammar
*/

#define MAX_LENGTH 200
// Token 記錄字面符號；AST 則使用同一組 Kind 表示運算節點。
// PLUS/MINUS 是一元正負號，ADD/SUB 是二元加減。
typedef enum {
	ASSIGN, ADD, SUB, MUL, DIV, REM, PREINC, PREDEC, POSTINC, POSTDEC, IDENTIFIER, CONSTANT, LPAR, RPAR, PLUS, MINUS, END
} Kind;
typedef enum {
	STMT, EXPR, ASSIGN_EXPR, ADD_EXPR, MUL_EXPR, UNARY_EXPR, POSTFIX_EXPR, PRI_EXPR
} GrammarState;
typedef struct TokenUnit {
	Kind kind;
	long long val; // integer literal or variable name
	struct TokenUnit *next;
} Token;
typedef struct ASTUnit {
	Kind kind;
	long long val; // integer literal or variable name
	struct ASTUnit *lhs, *mid, *rhs;
} AST;

/// utility interfaces

// err marco should be used when a expression error occurs.
#define err(x) {\
	puts("Compile Error!");\
	if(DEBUG) {\
		fprintf(stderr, "Error at line: %d\n", __LINE__);\
		fprintf(stderr, "Error message: %s\n", x);\
	}\
	exit(0);\
}
// You may set DEBUG=1 to debug. Remember setting back to 0 before submit.
#define DEBUG 0
// Split the input char array into token linked list.
Token *lexer(const char *in);
// Create a new token.
Token *new_token(Kind kind, long long val);
// Translate a token linked list into array, return its length.
size_t token_list_to_arr(Token **head);
// Parse the token array. Return the constructed AST.
AST *parser(Token *arr, size_t len);
// Parse the token array. Return the constructed AST.
AST *parse(Token *arr, int l, int r, GrammarState S);
// Create a new AST node.
AST *new_AST(Kind kind, long long val);
// Find the location of next token that fits the condition(cond). Return -1 if not found. Search direction from start to end.
int findNextSection(Token *arr, int start, int end, int (*cond)(Kind));
// Return 1 if kind is ASSIGN.
int condASSIGN(Kind kind);
// Return 1 if kind is ADD or SUB.
int condADD(Kind kind);
// Return 1 if kind is MUL, DIV, or REM.
int condMUL(Kind kind);
// Return 1 if kind is RPAR.
int condRPAR(Kind kind);
// Check if the AST is semantically right. This function will call err() automatically if check failed.
void semantic_check(AST *now);
// Generate ASM code.
void codegen(AST *root);
void emit_program(void);
// Free the whole AST.
void freeAST(AST *now);

char input[MAX_LENGTH];

int main(void) {
	// 每行各自解析並檢查，但變數的值 ID 會延續到下一行。
	// 所有行都處理完後，再輸出最終結果需要的組合語言。
	while (fgets(input, MAX_LENGTH, stdin) != NULL) {
		Token *content = lexer(input);
		size_t len = token_list_to_arr(&content);
		if (len == 0) continue;
		AST *ast_root = parser(content, len);
		// 先檢查合法性，避免常數折疊掩蓋 (1 + 2)++ 這類錯誤。
		semantic_check(ast_root);
		codegen(ast_root);
		free(content);
		freeAST(ast_root);
	}
	emit_program();
	return 0;
}

Token *lexer(const char *in) {
	Token *head = NULL;
	Token **now = &head;
	for (int i = 0; in[i]; i++) {
		if (isspace((unsigned char)in[i])) // ignore space characters
			continue;
		else if (isdigit((unsigned char)in[i])) {
			long long number = 0;
			int base = in[i] == '0' ? 8 : 10;
			do {
				int digit = in[i] - '0';
				number = number * base + digit;
				if (digit >= base || number > (long long)INT_MAX + 1)
					err("Invalid or out-of-range integer literal.");
				i++;
			} while (isdigit((unsigned char)in[i]));
			i--;
			(*now) = new_token(CONSTANT, number);
		}
		else if ('x' <= in[i] && in[i] <= 'z') // variable
			(*now) = new_token(IDENTIFIER, in[i]);
		else switch (in[i]) {
			case '=':
				(*now) = new_token(ASSIGN, 0);
				break;
			case '+':
				if (in[i+1] && in[i+1] == '+') {
					i++;
					// In lexer scope, all "++" will be labeled as PREINC.
					(*now) = new_token(PREINC, 0);
				}
				// In lexer scope, all single "+" will be labeled as PLUS.
				else (*now) = new_token(PLUS, 0);
				break;
			case '-':
				if (in[i+1] && in[i+1] == '-') {
					i++;
					// In lexer scope, all "--" will be labeled as PREDEC.
					(*now) = new_token(PREDEC, 0);
				}
				// In lexer scope, all single "-" will be labeled as MINUS.
				else (*now) = new_token(MINUS, 0);
				break;
			case '*':
				(*now) = new_token(MUL, 0);
				break;
			case '/':
				(*now) = new_token(DIV, 0);
				break;
			case '%':
				(*now) = new_token(REM, 0);
				break;
			case '(':
				(*now) = new_token(LPAR, 0);
				break;
			case ')':
				(*now) = new_token(RPAR, 0);
				break;
			case ';':
				(*now) = new_token(END, 0);
				break;
			default:
				err("Unexpected character.");
		}
		now = &((*now)->next);
	}
	return head;
}

Token *new_token(Kind kind, long long val) {
	Token *res = (Token*)malloc(sizeof(Token));
	if (res == NULL) err("Could not allocate token.");
	res->kind = kind;
	res->val = val;
	res->next = NULL;
	return res;
}

size_t token_list_to_arr(Token **head) {
	size_t res;
	Token *now = (*head), *del;
	for (res = 0; now != NULL; res++) now = now->next;
	now = (*head);
	if (res != 0) {
		*head = malloc(sizeof(Token) * res);
		if (*head == NULL) err("Could not allocate token array.");
	}
	for (size_t i = 0; i < res; i++) {
		(*head)[i] = (*now);
		del = now;
		now = now->next;
		free(del);
	}
	return res;
}

AST *parser(Token *arr, size_t len) {
	int depth = 0;
	for (size_t i = 0; i < len; i++) {
		if (arr[i].kind == LPAR) depth++;
		if (arr[i].kind == RPAR && --depth < 0)
			err("Unmatched closing parenthesis.");
		if (i == 0) continue;
		// correctly identify "ADD" and "SUB"
		if (arr[i].kind == PLUS || arr[i].kind == MINUS) {
			switch (arr[i - 1].kind) {
				case PREINC:
				case PREDEC:
				case IDENTIFIER:
				case CONSTANT:
				case RPAR:
					arr[i].kind = arr[i].kind - PLUS + ADD;
				default: break;
			}
		}
	}
	if (depth != 0)
		err("Unmatched opening parenthesis.");
	return parse(arr, 0, (int)len - 1, STMT);
}

AST *parse(Token *arr, int l, int r, GrammarState S) {
	// [l, r] 是目前解析的 token 範圍，S 是這次套用的文法層級。
	AST *now = NULL;
	if (l > r)
		err("Unexpected parsing range.");
	int nxt;
	switch (S) {
		case STMT:
			if (l == r && arr[l].kind == END)
				return NULL;
			else if (arr[r].kind == END)
				return parse(arr, l, r - 1, EXPR);
			else err("Expected \';\' at the end of line.");
		case EXPR:
			return parse(arr, l, r, ASSIGN_EXPR);
		case ASSIGN_EXPR:
			// 從左邊找等號，讓右側遞迴解析，得到 x = (y = 3)。
			if ((nxt = findNextSection(arr, l, r, condASSIGN)) != -1) {
				now = new_AST(arr[nxt].kind, 0);
				now->lhs = parse(arr, l, nxt - 1, UNARY_EXPR);
				now->rhs = parse(arr, nxt + 1, r, ASSIGN_EXPR);
				return now;
			}
			return parse(arr, l, r, ADD_EXPR);
		case ADD_EXPR:
			// 從右邊切開加減，讓左側遞迴解析，得到 (8 - 3) - 2。
			if((nxt = findNextSection(arr, r, l, condADD)) != -1) {
				now = new_AST(arr[nxt].kind, 0);
				now->lhs = parse(arr, l, nxt - 1, ADD_EXPR);
				now->rhs = parse(arr, nxt + 1, r, MUL_EXPR);
				return now;
			}
			return parse(arr, l, r, MUL_EXPR);
		case MUL_EXPR:
			if ((nxt = findNextSection(arr, r, l, condMUL)) != -1) {
				now = new_AST(arr[nxt].kind, 0);
				now->lhs = parse(arr, l, nxt - 1, MUL_EXPR);
				now->rhs = parse(arr, nxt + 1, r, UNARY_EXPR);
				return now;
			}
			return parse(arr, l, r, UNARY_EXPR);
		case UNARY_EXPR:
			if (arr[l].kind == PREINC || arr[l].kind == PREDEC ||
			    arr[l].kind == PLUS || arr[l].kind == MINUS) {
				now = new_AST(arr[l].kind, 0);
				now->mid = parse(arr, l + 1, r, UNARY_EXPR);
				return now;
			}
			return parse(arr, l, r, POSTFIX_EXPR);
		case POSTFIX_EXPR:
			if (arr[r].kind == PREINC || arr[r].kind == PREDEC) {
				// translate "PREINC", "PREDEC" into "POSTINC", "POSTDEC"
				now = new_AST(arr[r].kind - PREINC + POSTINC, 0);
				now->mid = parse(arr, l, r - 1, POSTFIX_EXPR);
				return now;
			}
			return parse(arr, l, r, PRI_EXPR);
		case PRI_EXPR:
			if (arr[l].kind == LPAR && findNextSection(arr, l, r, condRPAR) == r) {
				now = new_AST(LPAR, 0);
				now->mid = parse(arr, l + 1, r - 1, EXPR);
				return now;
			}
			if (l == r) {
				if (arr[l].kind == IDENTIFIER || arr[l].kind == CONSTANT)
					return new_AST(arr[l].kind, arr[l].val);
				err("Unexpected token during parsing.");
			}
			err("No token left for parsing.");
		default:
			err("Unexpected grammar state.");
	}
}

AST *new_AST(Kind kind, long long val) {
	AST *res = (AST*)malloc(sizeof(AST));
	if (res == NULL) err("Could not allocate AST node.");
	res->kind = kind;
	res->val = val;
	res->lhs = res->mid = res->rhs = NULL;
	return res;
}

int findNextSection(Token *arr, int start, int end, int (*cond)(Kind)) {
	// 只在括號深度回到 0 時尋找切割點，括號內交給下一層遞迴。
	int par = 0;
	int d = (start < end) ? 1 : -1;
	for (int i = start; (start < end) ? (i <= end) : (i >= end); i += d) {
		if (arr[i].kind == LPAR) par++;
		if (arr[i].kind == RPAR) par--;
		if (par == 0 && cond(arr[i].kind) == 1) return i;
	}
	return -1;
}

int condASSIGN(Kind kind) {
	return kind == ASSIGN;
}

int condADD(Kind kind) {
	return kind == ADD || kind == SUB;
}

int condMUL(Kind kind) {
	return kind == MUL || kind == DIV || kind == REM;
}

int condRPAR(Kind kind) {
	return kind == RPAR;
}

static const AST *without_parens(const AST *node) {
	while (node->kind == LPAR) node = node->mid;
	return node;
}

static int variable_index(const AST *node) {
	// ((x)) 仍然可以指定或遞增，但 (+x)、(x + y) 都不是合法目標。
	node = without_parens(node);
	if (node->kind != IDENTIFIER)
		err("Assignment and increment/decrement require a variable.");
	return (int)node->val - 'x';
}

void semantic_check(AST *now) {
	if (now == NULL) return;
	switch (now->kind) {
		case ASSIGN:
			variable_index(now->lhs);
			break;
		case PREINC: case PREDEC: case POSTINC: case POSTDEC:
			variable_index(now->mid);
			break;
		default:
			break;
	}
	semantic_check(now->lhs);
	semantic_check(now->mid);
	semantic_check(now->rhs);
}

#define VARIABLE_COUNT 3
#define REGISTER_COUNT 256
#define MAX_VALUES (15 * MAX_LENGTH + VARIABLE_COUNT)

typedef struct {
	Kind kind;
	int number; // CONSTANT 的數值，或初始變數的索引 0、1、2
	int lhs, rhs; // 運算來源的值 ID，並非暫存器編號
	int remaining_uses; // 尚未消耗的引用次數，降到 0 時可釋放暫存器
	int register_id; // 此值所在的暫存器；-1 表示尚未配置或已釋放
} Value;

// 每個值 ID 代表固定的運算結果；指定運算只更新變數對應的 ID。
// 因此 x = y; y++; 之後，x 仍能保留 y 的舊值。
// 最前面的 3 個值 ID 代表記憶體中的初始 x、y、z。
static Value values[MAX_VALUES] = {
	{.kind = IDENTIFIER, .number = 0, .register_id = -1},
	{.kind = IDENTIFIER, .number = 1, .register_id = -1},
	{.kind = IDENTIFIER, .number = 2, .register_id = -1}
};
static int value_count = VARIABLE_COUNT;
static int variable_values[VARIABLE_COUNT] = {0, 1, 2};
static int register_used[REGISTER_COUNT];

static int new_value(Kind kind, int number, int lhs, int rhs) {
	// 加法、乘法先統一來源順序，才能辨認 a + b 和 b + a。
	// 若只有一個來源是常數，將它放右側，方便使用立即數。
	if ((kind == ADD || kind == MUL) &&
	    ((values[lhs].kind == CONSTANT && values[rhs].kind != CONSTANT) ||
	     ((values[lhs].kind == CONSTANT) == (values[rhs].kind == CONSTANT) &&
	      lhs > rhs))) {
		int temporary = lhs;
		lhs = rhs;
		rhs = temporary;
	}
	// 運算種類和來源 ID 都相同，就共用既有結果。
	for (int id = VARIABLE_COUNT; id < value_count; id++) {
		if (values[id].kind == kind && values[id].number == number &&
		    values[id].lhs == lhs && values[id].rhs == rhs)
			return id;
	}
	if (value_count == MAX_VALUES) err("Too many values.");
	int id = value_count++;
	values[id] = (Value){.kind = kind, .number = number,
	                     .lhs = lhs, .rhs = rhs, .register_id = -1};
	return id;
}

static int constant(long long number) {
	// 轉成 ISA 使用的值時才檢查 32 位元範圍。
	if (number < INT_MIN || number > INT_MAX)
		err("Integer value out of range.");
	return new_value(CONSTANT, (int)number, 0, 0);
}

static int is_constant(int id, int number) {
	return values[id].kind == CONSTANT && values[id].number == number;
}

// 常數折疊與值傳遞共用的運算，先檢查溢位與除以零。
static long long evaluate_constant(Kind kind, long long a, long long b) {
	switch (kind) {
		case ADD:
			if ((b > 0 && a > LLONG_MAX - b) ||
			    (b < 0 && a < LLONG_MIN - b))
				err("Constant expression overflow.");
			return a + b;
		case SUB:
			if ((b < 0 && a > LLONG_MAX + b) ||
			    (b > 0 && a < LLONG_MIN + b))
				err("Constant expression overflow.");
			return a - b;
		case MUL:
			if ((a > 0 && b > 0 && a > LLONG_MAX / b) ||
			    (a > 0 && b < 0 && b < LLONG_MIN / a) ||
			    (a < 0 && b > 0 && a < LLONG_MIN / b) ||
			    (a < 0 && b < 0 && a < LLONG_MAX / b))
				err("Constant expression overflow.");
			return a * b;
		case DIV: case REM:
			if (b == 0) err("Division by zero.");
			if (a == LLONG_MIN && b == -1)
				err("Constant expression overflow.");
			return kind == DIV ? a / b : a % b;
		default:
			err("Unexpected arithmetic operator.");
	}
}

static int arithmetic(Kind kind, int lhs, int rhs) {
	if (values[lhs].kind == CONSTANT && values[rhs].kind == CONSTANT) {
		return constant(evaluate_constant(kind, values[lhs].number, values[rhs].number));
	}

	// 將常數移到右邊，再把 (y + 10) + 3 這類運算合併成 y + 13。
	if ((kind == ADD || kind == MUL) && values[lhs].kind == CONSTANT) {
		int temporary = lhs;
		lhs = rhs;
		rhs = temporary;
	}
	if ((kind == ADD || kind == SUB) && values[rhs].kind == CONSTANT &&
	    (values[lhs].kind == ADD || values[lhs].kind == SUB)) {
		int base = values[lhs].lhs;
		int offset = values[lhs].rhs;
		if (values[lhs].kind == ADD && values[base].kind == CONSTANT) {
			int temporary = base;
			base = offset;
			offset = temporary;
		}
		if (values[offset].kind == CONSTANT) {
			long long combined = values[offset].number;
			if (values[lhs].kind == SUB) combined = -combined;
			combined += kind == ADD ? (long long)values[rhs].number
			                        : -(long long)values[rhs].number;
			// INT_MIN 需要兩條指令建立，合併成它可能反而增加 cycle。
			if (combined > INT_MIN && combined <= INT_MAX)
				return arithmetic(ADD, base, constant(combined));
		}
	}

	// 指定、遞增、遞減的效果已由 expression_value 記錄。
	// 即使把 (y++) * 0 簡化成 0，仍會保留 y 的更新。
	switch (kind) {
		case ADD:
			if (is_constant(lhs, 0)) return rhs;
			if (is_constant(rhs, 0)) return lhs;
			break;
		case SUB:
			if (lhs == rhs) return constant(0);
			if (is_constant(rhs, 0)) return lhs;
			if (is_constant(lhs, 0) && values[rhs].kind == SUB &&
			    is_constant(values[rhs].lhs, 0)) return values[rhs].rhs;
			if (values[lhs].kind == ADD) {
				if (values[lhs].lhs == rhs) return values[lhs].rhs;
				if (values[lhs].rhs == rhs) return values[lhs].lhs;
			}
			break;
		case MUL:
			if (is_constant(lhs, 0) || is_constant(rhs, 1)) return lhs;
			if (is_constant(rhs, 0) || is_constant(lhs, 1)) return rhs;
			if (is_constant(rhs, 2)) return arithmetic(ADD, lhs, lhs);
			if (is_constant(rhs, -1)) return arithmetic(SUB, constant(0), lhs);
			break;
		case DIV:
			if (lhs == rhs) return constant(1);
			if (is_constant(rhs, 1)) return lhs;
			if (is_constant(rhs, -1)) return arithmetic(SUB, constant(0), lhs);
			break;
		case REM:
			if (lhs == rhs || is_constant(rhs, 1) || is_constant(rhs, -1))
				return constant(0);
			break;
		default:
			err("Unexpected arithmetic operator.");
	}
	return new_value(kind, 0, lhs, rhs);
}

static int expression_value(const AST *node) {
	// 回傳這個運算式的值 ID，同時更新指定和遞增遞減影響的變數。
	int lhs, rhs, variable, old_value, new_id;
	const AST *operand;
	switch (node->kind) {
		case IDENTIFIER:
			return variable_values[variable_index(node)];
		case CONSTANT:
			return constant(node->val);
		case LPAR: case PLUS:
			return expression_value(node->mid);
		case MINUS:
			operand = without_parens(node->mid);
			if (operand->kind == CONSTANT) return constant(-operand->val);
			rhs = expression_value(operand);
			return arithmetic(SUB, constant(0), rhs);
		case ASSIGN:
			variable = variable_index(node->lhs);
			new_id = expression_value(node->rhs);
			variable_values[variable] = new_id;
			return new_id;
		case PREINC: case PREDEC: case POSTINC: case POSTDEC:
			// 兩者都更新變數；後置回傳舊值，前置回傳新值。
			variable = variable_index(node->mid);
			old_value = variable_values[variable];
			new_id = arithmetic(node->kind == PREINC || node->kind == POSTINC
			                    ? ADD : SUB, old_value, constant(1));
			variable_values[variable] = new_id;
			return node->kind == POSTINC || node->kind == POSTDEC ? old_value : new_id;
		case ADD: case SUB: case MUL: case DIV: case REM:
			lhs = expression_value(node->lhs);
			rhs = expression_value(node->rhs);
			return arithmetic(node->kind, lhs, rhs);
		default:
			err("Unexpected AST node in code generation.");
	}
}

// 由下往上折疊純常數子樹，再交給 32 位元的 ISA 值處理。
// 例如 2147483648 / 2 的中間常數超過 INT_MAX，但結果可以表示。
// 計算前先檢查 long long 溢位與除以零，避免編譯器本身發生未定義行為。
static void fold_literal_expression(AST *node) {
	if (node == NULL) return;
	fold_literal_expression(node->lhs);
	fold_literal_expression(node->mid);
	fold_literal_expression(node->rhs);

	long long result;
	if (node->mid != NULL && node->mid->kind == CONSTANT &&
	    (node->kind == LPAR || node->kind == PLUS || node->kind == MINUS)) {
		result = node->mid->val;
		if (node->kind == MINUS) {
			if (result == LLONG_MIN) err("Constant expression overflow.");
			result = -result;
		}
	} else if (node->lhs != NULL && node->rhs != NULL &&
	           node->lhs->kind == CONSTANT && node->rhs->kind == CONSTANT) {
		long long a = node->lhs->val, b = node->rhs->val;
		result = evaluate_constant(node->kind, a, b);
	} else {
		return;
	}
	freeAST(node->lhs);
	freeAST(node->mid);
	freeAST(node->rhs);
	node->lhs = node->mid = node->rhs = NULL;
	node->kind = CONSTANT;
	node->val = result;
}

void codegen(AST *root) {
	fold_literal_expression(root);
	if (root != NULL) expression_value(root);
}

// 從最終寫回的值往回計算引用次數，沒有被走訪的運算就能省略。
// 共用節點每次都增加引用，但其來源只在首次走訪時展開。
static void count_use(int id) {
	Value *value = &values[id];
	if (value->remaining_uses++ == 0 && value->kind != IDENTIFIER && value->kind != CONSTANT) {
		count_use(value->lhs);
		count_use(value->rhs);
	}
}

static int allocate_register(void) {
	// 優先配置 r0～r7，減少高編號暫存器造成的 cycle 懲罰。
	for (int reg = 0; reg < REGISTER_COUNT; reg++) {
		if (!register_used[reg]) {
			register_used[reg] = 1;
			return reg;
		}
	}
	err("Out of registers.");
}

static void release_use(int id) {
	Value *value = &values[id];
	if (--value->remaining_uses == 0 && value->register_id >= 0) {
		register_used[value->register_id] = 0;
		value->register_id = -1;
	}
}

static int emit_value(int id);

static void format_operand(int id, char *text) {
	// 非負常數可直接作為立即數；負數與其他值需要暫存器。
	if (values[id].kind == CONSTANT && values[id].number >= 0)
		sprintf(text, "%d", values[id].number);
	else
		sprintf(text, "r%d", emit_value(id));
}

static int emit_value(int id) {
	Value *value = &values[id];
	if (value->register_id >= 0) return value->register_id;
	if (value->kind == IDENTIFIER) {
		value->register_id = allocate_register();
		printf("load r%d [%d]\n", value->register_id, value->number * 4);
	} else if (value->kind == CONSTANT) {
		value->register_id = allocate_register();
		if (value->number == INT_MIN) {
			printf("sub r%d 0 %d\n", value->register_id, INT_MAX);
			printf("sub r%d r%d 1\n", value->register_id, value->register_id);
		} else if (value->number < 0) {
			printf("sub r%d 0 %d\n", value->register_id, -value->number);
		} else {
			printf("add r%d 0 %d\n", value->register_id, value->number);
		}
	} else {
		static const char *const opcode[] = {
			[ADD] = "add", [SUB] = "sub", [MUL] = "mul",
			[DIV] = "div", [REM] = "rem"
		};
		Kind kind = value->kind;
		int lhs = value->lhs, rhs = value->rhs;
		char left_text[16], right_text[16];
		// 例如 a + (-5) 改用 a - 5，省去建立負數暫存器的指令。
		int right_number = values[rhs].number;
		if ((kind == ADD || kind == SUB) && values[rhs].kind == CONSTANT &&
		    right_number < 0 && right_number != INT_MIN) {
			kind = kind == ADD ? SUB : ADD;
			format_operand(lhs, left_text);
			sprintf(right_text, "%d", -right_number);
		} else {
			format_operand(lhs, left_text);
			format_operand(rhs, right_text);
		}
		// 先保存來源文字，再釋放已用完的來源。
		// ISA 允許目的暫存器和來源相同，例如 add r0 r0 5。
		release_use(lhs);
		release_use(rhs);
		value->register_id = allocate_register();
		printf("%s r%d %s %s\n", opcode[kind], value->register_id, left_text, right_text);
	}
	return value->register_id;
}

void emit_program(void) {
	for (int i = 0; i < VARIABLE_COUNT; i++) {
		if (variable_values[i] != i) count_use(variable_values[i]);
	}
	// 來源 ID 一定早於使用它的運算 ID，依序輸出可先準備好來源。
	// 只有 remaining_uses 不為 0 的值才會影響最終結果。
	for (int id = 0; id < value_count; id++) {
		if (values[id].remaining_uses != 0 && values[id].kind != CONSTANT)
			emit_value(id);
	}
	// 此時需要的初值都已載入，寫回不會覆蓋尚未讀取的初始資料。
	for (int i = 0; i < VARIABLE_COUNT; i++) {
		int id = variable_values[i];
		if (id == i) continue;
		printf("store [%d] r%d\n", i * 4, emit_value(id));
		release_use(id);
	}
}

void freeAST(AST *now) {
	if (now == NULL) return;
	freeAST(now->lhs);
	freeAST(now->mid);
	freeAST(now->rhs);
	free(now);
}
